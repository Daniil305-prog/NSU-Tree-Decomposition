#include "heuristics.h"
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Предварительные объявления статических вспомогательных функций (доступны только внутри этого файла)
static void nd_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx);
static void hybrid_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx);

/**
 * Вспомогательная функция для глубокого копирования (клонирования) графа.
 * Нужна для эвристик, которые симулируют удаление вершин и достраивают хорды (Fill-in),
 * чтобы не испортить исходную структуру графа.
 */
static Graph *clone_graph(const Graph *orig)
{
	Graph *copy = create_graph(orig->num_vertices);
	if (!copy)
		return NULL;

	// Копируем только уникальные рёбра (чтобы не дублировать их, учитывая u < v)
	for (int u = 1; u <= orig->num_vertices; u++)
	{
		for (int i = 0; i < orig->adj[u].degree; i++)
		{
			int v = orig->adj[u].neighbors[i];
			if (u < v)
				add_edge(copy, u, v);
		}
	}
	return copy;
}

/**
 * 1. ЭВРИСТИКА: Minimum Degree (Минимальная степень)
 * Стратегия: На каждом шаге выбираем вершину с наименьшим числом живых соседей,
 * удаляем её и соединяем всех её соседей между собой (превращаем их в клику).
 */
int *min_degree_ordering(const Graph *g)
{
	Graph *working_g = clone_graph(g); // Создаем рабочую копию графа для симуляции удаления
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));	// Итоговый массив порядка исключения
	bool *eliminated = (bool *)calloc(n + 1, sizeof(bool)); // Флаги "удаленных" вершин

	for (int step = 1; step <= n; step++)
	{
		int min_deg = -1, best_v = -1;

		// Находим среди оставшихся вершин ту, у которой минимальная активная степень
		for (int v = 1; v <= n; v++)
		{
			if (!eliminated[v])
			{
				int active_degree = 0;
				// Считаем только тех соседей, которые ещё не были удалены
				for (int i = 0; i < working_g->adj[v].degree; i++)
				{
					if (!eliminated[working_g->adj[v].neighbors[i]])
						active_degree++;
				}

				if (min_deg == -1 || active_degree < min_deg)
				{
					min_deg = active_degree;
					best_v = v;
				}
			}
		}

		ordering[step] = best_v;   // Записываем найденную вершину в текущую позицию порядка
		eliminated[best_v] = true; // Помечаем вершину как удаленную

		// Симуляция исключения: соединяем всех активных соседей вершины best_v между собой
		if (working_g->adj[best_v].degree > 0)
		{
			// Собираем список всех живых соседей исключаемой вершины
			int *active_neighbors = (int *)malloc(working_g->adj[best_v].degree * sizeof(int));
			int count = 0;
			for (int i = 0; i < working_g->adj[best_v].degree; i++)
			{
				int neighbor = working_g->adj[best_v].neighbors[i];
				if (!eliminated[neighbor])
					active_neighbors[count++] = neighbor;
			}

			// Попарно соединяем их рёбрами (хордами), если между ними ещё нет связи
			for (int i = 0; i < count; i++)
			{
				for (int j = i + 1; j < count; j++)
				{
					if (!has_edge(working_g, active_neighbors[i], active_neighbors[j]))
					{
						add_edge(working_g, active_neighbors[i], active_neighbors[j]);
					}
				}
			}
			free(active_neighbors);
		}
	}

	// Освобождаем временную память
	free(eliminated);
	free_graph(working_g);
	return ordering;
}

/**
 * 2. ЭВРИСТИКА: Maximum Cardinality Search (MCS / Поиск максимальной мощности)
 * Стратегия: Идём с конца порядка (от n до 1). Выбираем вершину, которая имеет
 * наибольшее количество уже посещенных (выбранных на будущие шаги) соседей.
 * Гарантирует идеальный (аккордный) граф, если исходный граф уже был хордальным.
 */
int *mcs_ordering(const Graph *g)
{
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *visited = (bool *)calloc(n + 1, sizeof(bool)); // Флаги задействованных вершин
	int *weight = (int *)calloc(n + 1, sizeof(int));	 // Вес = количество посещенных соседей

	// Заполняем массив порядка строго задом наперёд
	for (int step = n; step >= 1; step--)
	{
		int max_weight = -1, best_v = -1;

		// Ищем непосещенную вершину с максимальным текущим весом
		for (int v = 1; v <= n; v++)
		{
			if (!visited[v] && weight[v] > max_weight)
			{
				max_weight = weight[v];
				best_v = v;
			}
		}

		// Защита: если у всех оставшихся вершин веса равны 0 (например, граф несвязный),
		// просто берем первую попавшуюся свободную вершину
		if (best_v == -1)
		{
			for (int v = 1; v <= n; v++)
			{
				if (!visited[v])
				{
					best_v = v;
					break;
				}
			}
		}

		ordering[step] = best_v; // Прописываем в порядок
		visited[best_v] = true;	 // Маркируем как посещенную

		// Наращиваем веса всем непосещенным соседям выбранной вершины
		for (int i = 0; i < g->adj[best_v].degree; i++)
		{
			int neighbor = g->adj[best_v].neighbors[i];
			if (!visited[neighbor])
				weight[neighbor]++;
		}
	}

	free(visited);
	free(weight);
	return ordering;
}

/**
 * Локальная версия алгоритма MCS, работающая исключительно внутри выделенного подграфа.
 * Используется Гибридным алгоритмом, когда размер задачи сжимается до порогового значения.
 */
static void local_mcs_for_hybrid(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx)
{
	// Строим маску (быстрый фильтр), чтобы мгновенно проверять принадлежность вершины к подграфу
	bool *sub_mask = (bool *)calloc(g->num_vertices + 1, sizeof(bool));
	for (int i = 0; i < sub_count; i++)
		sub_mask[sub_vertices[i]] = true;

	int *weight = (int *)calloc(g->num_vertices + 1, sizeof(int));
	bool *local_visited = (bool *)calloc(g->num_vertices + 1, sizeof(bool));

	for (int step = 0; step < sub_count; step++)
	{
		int max_weight = -1, best_v = -1;

		// Ищем лидера по весу среди вершин, принадлежащих данному подмножеству
		for (int i = 0; i < sub_count; i++)
		{
			int v = sub_vertices[i];
			if (!local_visited[v] && !global_placed[v])
			{
				if (weight[v] > max_weight)
				{
					max_weight = weight[v];
					best_v = v;
				}
			}
		}

		// Обработка изолятов внутри подмножества
		if (best_v == -1)
		{
			for (int i = 0; i < sub_count; i++)
			{
				int v = sub_vertices[i];
				if (!local_visited[v] && !global_placed[v])
				{
					best_v = v;
					break;
				}
			}
		}
		if (best_v == -1)
			break;

		// Фиксируем вершину в глобальном массиве порядка
		ordering[*current_idx] = best_v;
		(*current_idx)--; // Двигаем индекс заполнения глобального порядка влево
		local_visited[best_v] = true;
		global_placed[best_v] = true;

		// Обновляем веса соседей, но строго ограничиваясь рамками текущего подграфа
		for (int i = 0; i < g->adj[best_v].degree; i++)
		{
			int neighbor = g->adj[best_v].neighbors[i];
			if (sub_mask[neighbor] && !global_placed[neighbor] && !local_visited[neighbor])
			{
				weight[neighbor]++;
			}
		}
	}
	free(sub_mask);
	free(weight);
	free(local_visited);
}

/**
 * 3. ЭВРИСТИКА: Nested Dissection (Вложенные сечения)
 * Стратегия: Подход "Разделяй и властвуй". Находим в графе множество вершин-разделяющих (сепаратор),
 * удаление которых разбивает граф на независимые части А и Б.
 * Сепаратор помещается в КОНЕЦ порядка, а части А и Б рекурсивно обрабатываются тем же методом.
 */
int *nested_dissection_ordering(const Graph *g)
{
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *global_placed = (bool *)calloc(n + 1, sizeof(bool)); // Контроль распределения вершин

	// Изначально подмножество включает в себя абсолютно все вершины графа
	int *all_vertices = (int *)malloc(n * sizeof(int));
	for (int i = 0; i < n; i++)
		all_vertices[i] = i + 1;

	int current_idx = n; // Заполнение порядка начинаем с самого конца (индекс n)
	nd_helper(g, all_vertices, n, global_placed, ordering, &current_idx);

	// Страховочный цикл: если из-за жесткой геометрии графа какие-то вершины выпали из рекурсии,
	// дописываем их в свободные слоты начала порядка.
	for (int v = 1; v <= n; v++)
	{
		if (!global_placed[v])
		{
			ordering[current_idx] = v;
			current_idx--;
		}
	}
	free(global_placed);
	free(all_vertices);
	return ordering;
}

/**
 * Рекурсивная функция-помощник для Nested Dissection.
 * Ищет псевдо-сепаратор с помощью геометрических уровней BFS обхода.
 */
static void nd_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx)
{
	if (sub_count == 0)
		return;

	// Базовый случай рекурсии: осталась одна вершина
	if (sub_count == 1)
	{
		int v = sub_vertices[0];
		if (!global_placed[v])
		{
			ordering[*current_idx] = v;
			(*current_idx)--;
			global_placed[v] = true;
		}
		return;
	}

	// Запускаем алгоритм BFS для построения дерева слоев и поиска центрального сепаратора
	bool *local_visited = (bool *)calloc(g->num_vertices + 1, sizeof(bool));
	int *queue = (int *)malloc(sub_count * sizeof(int));
	int head = 0, tail = 0;

	int start_v = sub_vertices[0];
	queue[tail++] = start_v;
	local_visited[start_v] = true;

	int *level = (int *)calloc(g->num_vertices + 1, sizeof(int)); // Массив уровней удаленности от корня BFS
	int max_level = 0;

	while (head < tail)
	{
		int u = queue[head++];
		if (level[u] > max_level)
			max_level = level[u];

		for (int i = 0; i < g->adj[u].degree; i++)
		{
			int v = g->adj[u].neighbors[i];

			// Проверяем, находится ли сосед в нашем текущем рекурсивном подмножестве
			bool in_sub = false;
			for (int k = 0; k < sub_count; k++)
			{
				if (sub_vertices[k] == v)
				{
					in_sub = true;
					break;
				}
			}

			if (in_sub && !local_visited[v] && !global_placed[v])
			{
				local_visited[v] = true;
				level[v] = level[u] + 1; // Уровень соседа на 1 больше родительского
				queue[tail++] = v;
			}
		}
	}

	// Геометрический сепаратор: берем слой, находящийся ровно посередине BFS-дерева
	int sep_level = max_level / 2;
	if (sep_level == 0 && max_level > 0)
		sep_level = 1;

	// Выделяем память под три составные части разделения
	int *sep_nodes = (int *)malloc(sub_count * sizeof(int));
	int *part_A = (int *)malloc(sub_count * sizeof(int));
	int *part_B = (int *)malloc(sub_count * sizeof(int));
	int sep_count = 0, a_count = 0, b_count = 0;

	// Сортируем вершины по трем карманам на основе их BFS-уровня
	for (int i = 0; i < sub_count; i++)
	{
		int v = sub_vertices[i];
		if (local_visited[v] && level[v] == sep_level)
			sep_nodes[sep_count++] = v; // Средний слой -> становится сепаратором
		else if (local_visited[v] && level[v] < sep_level)
			part_A[a_count++] = v; // Левая часть графа
		else
			part_B[b_count++] = v; // Правая часть графа (туда же идут недостижимые в BFS вершины)
	}

	// Если сепаратор пуст, искусственно назначаем сепаратором первую вершину
	if (sep_count == 0)
	{
		sep_nodes[sep_count++] = sub_vertices[0];
		for (int i = 1; i < sub_count; i++)
			part_B[b_count++] = sub_vertices[i];
	}

	// Размещаем вершины сепаратора в текущие верхние позиции порядка (они обрабатываются позже всего)
	for (int i = 0; i < sep_count; i++)
	{
		int v = sep_nodes[i];
		if (!global_placed[v])
		{
			ordering[*current_idx] = v;
			(*current_idx)--;
			global_placed[v] = true;
		}
	}

	free(local_visited);
	free(queue);
	free(level);
	free(sep_nodes);

	// Рекурсивно углубляемся в обе изолированные половины графа
	nd_helper(g, part_B, b_count, global_placed, ordering, current_idx);
	nd_helper(g, part_A, a_count, global_placed, ordering, current_idx);

	free(part_A);
	free(part_B);
}

/**
 * 4. ЭВРИСТИКА: Minimum Fill-In (Минимальное добавление ребер)
 * Стратегия: Самая "умная" локальная эвристика. На каждом шаге ищет вершину,
 * исключение которой добавит МИНИМАЛЬНОЕ количество новых хорд (лишних рёбер).
 * Позволяет удерживать мешки древовидной декомпозиции максимально компактными.
 */
int *min_fill_in_ordering(const Graph *g)
{
	Graph *working_g = clone_graph(g); // Копируем граф для динамического изменения рёбер
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *eliminated = (bool *)calloc(n + 1, sizeof(bool));

	for (int step = 1; step <= n; step++)
	{
		int min_fill = -1, best_v = -1;

		// Сканируем все живые вершины
		for (int v = 1; v <= n; v++)
		{
			if (!eliminated[v])
			{
				// Считаем число её активных соседей
				int active_count = 0;
				for (int i = 0; i < working_g->adj[v].degree; i++)
				{
					if (!eliminated[working_g->adj[v].neighbors[i]])
						active_count++;
				}

				int local_fill = 0;
				// Моделируем исключение вершины v: считаем, сколько ребер не хватает её соседям, чтобы стать кликой
				if (active_count > 1)
				{
					int *temp_neigh = (int *)malloc(working_g->adj[v].degree * sizeof(int));
					int tc = 0;
					for (int i = 0; i < working_g->adj[v].degree; i++)
					{
						int nb = working_g->adj[v].neighbors[i];
						if (!eliminated[nb])
							temp_neigh[tc++] = nb;
					}

					// Проверяем каждую пару соседей на наличие ребра между ними
					for (int i = 0; i < tc; i++)
					{
						for (int j = i + 1; j < tc; j++)
						{
							if (!has_edge(working_g, temp_neigh[i], temp_neigh[j]))
								local_fill++; // Если ребра нет, фиксируем необходимость добавления хорды
						}
					}
					free(temp_neigh);
				}

				// Ищем вершину с абсолютным минимумом добавленных рёбер (идеально, если fill-in == 0)
				if (min_fill == -1 || local_fill < min_fill)
				{
					min_fill = local_fill;
					best_v = v;
				}
			}
		}

		ordering[step] = best_v;
		eliminated[best_v] = true;

		// Фиксируем изменения в структуре рабочего графа: фактически добавляем недостающие ребра (хорды)
		if (working_g->adj[best_v].degree > 0)
		{
			int *active_neighbors = (int *)malloc(working_g->adj[best_v].degree * sizeof(int));
			int count = 0;
			for (int i = 0; i < working_g->adj[best_v].degree; i++)
			{
				int neighbor = working_g->adj[best_v].neighbors[i];
				if (!eliminated[neighbor])
					active_neighbors[count++] = neighbor;
			}
			for (int i = 0; i < count; i++)
			{
				for (int j = i + 1; j < count; j++)
				{
					if (!has_edge(working_g, active_neighbors[i], active_neighbors[j]))
					{
						add_edge(working_g, active_neighbors[i], active_neighbors[j]);
					}
				}
			}
			free(active_neighbors);
		}
	}
	free(eliminated);
	free_graph(working_g);
	return ordering;
}

/**
 * 5. ЭВРИСТИКА: Гибридный алгоритм (Nested Dissection + MCS)
 * Стратегия: Идеальный баланс скорости и качества. На больших объемах данных алгоритм
 * работает как глобальный Nested Dissection (быстро режет граф на независимые куски),
 * но как только размер куска падает ниже критического порога (<= 25 вершин), вместо медленной рекурсии
 * запускается локальный MCS, который идеально и точно упаковывает микро-граф.
 */
int *hybrid_nd_mcs_ordering(const Graph *g)
{
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *global_placed = (bool *)calloc(n + 1, sizeof(bool));
	int *all_vertices = (int *)malloc(n * sizeof(int));
	for (int i = 0; i < n; i++)
		all_vertices[i] = i + 1;

	int current_idx = n;
	hybrid_helper(g, all_vertices, n, global_placed, ordering, &current_idx);

	// Досборка пропущенных элементов структуры
	for (int v = 1; v <= n; v++)
	{
		if (!global_placed[v])
		{
			ordering[current_idx] = v;
			current_idx--;
		}
	}
	free(global_placed);
	free(all_vertices);
	return ordering;
}

/**
 * Рекурсивный движок гибридного метода с точкой переключения алгоритмов (Threshold).
 */
static void hybrid_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx)
{
	if (sub_count == 0)
		return;

	// === КЛЮЧЕВАЯ ОСОБЕННОСТЬ ГИБРИДА ===
	// Порог переключения: если локальный фрагмент графа содержит 25 или меньше вершин,
	// мы останавливаем деление и переключаемся на точный локальный алгоритм Maximum Cardinality Search.
	if (sub_count <= 25)
	{
		local_mcs_for_hybrid(g, sub_vertices, sub_count, global_placed, ordering, current_idx);
		return;
	}

	// Если кусок всё ещё большой, продолжаем стандартный алгоритм вложенных сечений (BFS-сепаратор)
	bool *local_visited = (bool *)calloc(g->num_vertices + 1, sizeof(bool));
	int *queue = (int *)malloc(sub_count * sizeof(int));
	int head = 0, tail = 0;
	int start_v = sub_vertices[0];
	queue[tail++] = start_v;
	local_visited[start_v] = true;

	int *level = (int *)calloc(g->num_vertices + 1, sizeof(int));
	int max_level = 0;

	while (head < tail)
	{
		int u = queue[head++];
		if (level[u] > max_level)
			max_level = level[u];
		for (int i = 0; i < g->adj[u].degree; i++)
		{
			int v = g->adj[u].neighbors[i];
			bool in_sub = false;
			for (int k = 0; k < sub_count; k++)
			{
				if (sub_vertices[k] == v)
				{
					in_sub = true;
					break;
				}
			}
			if (in_sub && !local_visited[v] && !global_placed[v])
			{
				local_visited[v] = true;
				level[v] = level[u] + 1;
				queue[tail++] = v;
			}
		}
	}

	int sep_level = max_level / 2;
	if (sep_level == 0 && max_level > 0)
		sep_level = 1;

	int *sep_nodes = (int *)malloc(sub_count * sizeof(int));
	int *part_A = (int *)malloc(sub_count * sizeof(int));
	int *part_B = (int *)malloc(sub_count * sizeof(int));
	int sep_count = 0, a_count = 0, b_count = 0;

	for (int i = 0; i < sub_count; i++)
	{
		int v = sub_vertices[i];
		if (local_visited[v] && level[v] == sep_level)
			sep_nodes[sep_count++] = v;
		else if (local_visited[v] && level[v] < sep_level)
			part_A[a_count++] = v;
		else
			part_B[b_count++] = v;
	}

	if (sep_count == 0)
	{
		sep_nodes[sep_count++] = sub_vertices[0];
		for (int i = 1; i < sub_count; i++)
			part_B[b_count++] = sub_vertices[i];
	}

	for (int i = 0; i < sep_count; i++)
	{
		int v = sep_nodes[i];
		if (!global_placed[v])
		{
			ordering[*current_idx] = v;
			(*current_idx)--;
			global_placed[v] = true;
		}
	}

	free(local_visited);
	free(queue);
	free(level);
	free(sep_nodes);

	// Рекурсивный спуск по разделенным ветвям гибрида
	hybrid_helper(g, part_B, b_count, global_placed, ordering, current_idx);
	hybrid_helper(g, part_A, a_count, global_placed, ordering, current_idx);
	free(part_A);
	free(part_B);
}