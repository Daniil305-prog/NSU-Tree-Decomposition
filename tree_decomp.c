#include "tree_decomp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * Локальная копия функции клонирования графа.
 * Необходима для построения хордального дополнения на временной структуре,
 * чтобы не разрушить исходный граф, переданный из main.c.
 */
static Graph *local_clone_graph(const Graph *orig)
{
	Graph *copy = create_graph(orig->num_vertices);
	for (int u = 1; u <= orig->num_vertices; u++)
	{
		for (int i = 0; i < orig->adj[u].degree; i++)
		{
			int v = orig->adj[u].neighbors[i];
			if (u < v) // Добавляем ребро один раз, чтобы избежать дублирования
				add_edge(copy, u, v);
		}
	}
	return copy;
}

/**
 * Функция вычисления хордального дополнения (Chordal Completion).
 * Принимает граф и эвристический порядок исключения вершин.
 * Симулирует процесс исключения вершин, считает ширину декомпозиции (treewidth)
 * и количество добавленных хорд (fill-in).
 */
OrderingMetrics compute_chordal_completion(const Graph *g, const int *ordering)
{
	int n = g->num_vertices;
	Graph *working_g = local_clone_graph(g);

	// Массив обратного поиска: pos[v] возвращает позицию (шаг), на котором вершина v будет удалена
	int *pos = (int *)malloc((n + 1) * sizeof(int));
	for (int i = 1; i <= n; i++)
		pos[ordering[i]] = i;

	int max_active_degree = 0; // Максимальная активная степень (определит итоговую ширину treewidth)
	int fill_in_count = 0;	   // Счетчик добавленных не достающих ребер (хорд)

	// Симулируем процесс исключения по заданному порядку ordering
	for (int i = 1; i <= n; i++)
	{
		int u = ordering[i]; // Текущая исключаемая вершина

		// Массив для сбора соседей вершины u, которые будут исключены ПОЗЖЕ неё
		int *higher_neighbors = (int *)malloc(working_g->adj[u].degree * sizeof(int));
		int count = 0;

		// Фильтруем соседей: собираем только тех, у кого позиция в порядке больше текущей
		for (int j = 0; j < working_g->adj[u].degree; j++)
		{
			int v = working_g->adj[u].neighbors[j];
			if (pos[v] > pos[u])
				higher_neighbors[count++] = v;
		}

		// treewidth графа равен максимальному числу таких "верхних" соседей по всем шагам
		if (count > max_active_degree)
			max_active_degree = count;

		// Математическое требование: превращаем всех "верхних" соседей в клику (соединяем попарно).
		// Если ребра между ними нет — это хорда (fill-in).
		for (int x = 0; x < count; x++)
		{
			for (int y = x + 1; y < count; y++)
			{
				if (!has_edge(working_g, higher_neighbors[x], higher_neighbors[y]))
				{
					add_edge(working_g, higher_neighbors[x], higher_neighbors[y]);
					fill_in_count++; // Фиксируем добавление новой хорды
				}
			}
		}
		free(higher_neighbors);
	}

	free(pos);
	free_graph(working_g);

	// Заполняем структуру метрик для возврата в main.c
	OrderingMetrics metrics;
	metrics.treewidth = max_active_degree;
	metrics.fill_in = fill_in_count;
	return metrics;
}

/**
 * Валидатор древовидной декомпозиции.
 * Проверяет построенную структуру мешков и дерева на строгое соответствие
 * трем фундаментальным математическим аксиомам древовидной декомпозиции.
 */
bool validate_tree_decomposition(const Graph *g, const int *ordering)
{
	int n = g->num_vertices;
	Graph *working_g = local_clone_graph(g);

	// Строим массив позиций для быстрого сравнения "кто удаляется позже"
	int *pos = (int *)malloc((n + 1) * sizeof(int));
	for (int i = 1; i <= n; i++)
		pos[ordering[i]] = i;

	// Выделяем структуры для хранения мешков (bags) древесной декомпозиции
	int **bags = (int **)malloc((n + 1) * sizeof(int *));
	int *bag_sizes = (int *)calloc(n + 1, sizeof(int));
	int *parent = (int *)calloc(n + 1, sizeof(int)); // Массив предков, задающий структуру дерева декомпозиции

	// === Шаг 1. Построение мешков и структуры дерева на основе дерева исключения (Elimination Tree) ===
	for (int i = 1; i <= n; i++)
	{
		int u = ordering[i];
		int *higher_neighbors = (int *)malloc(working_g->adj[u].degree * sizeof(int));
		int count = 0;

		for (int j = 0; j < working_g->adj[u].degree; j++)
		{
			int v = working_g->adj[u].neighbors[j];
			if (pos[v] > pos[u])
				higher_neighbors[count++] = v;
		}

		// Формируем мешок B_u: в него входит сама вершина u и все её "верхние" соседи
		bag_sizes[u] = count + 1;
		bags[u] = (int *)malloc(bag_sizes[u] * sizeof(int));
		bags[u][0] = u;
		for (int j = 0; j < count; j++)
			bags[u][j + 1] = higher_neighbors[j];

		// Задаем структуру дерева: родителем мешка u становится мешок той вершины w из higher_neighbors,
		// которая будет исключена ПЕРВОЙ среди всех остальных соседей (минимальная позиция)
		int min_p = n + 2;
		int best_parent = 0;
		for (int j = 0; j < count; j++)
		{
			int w = higher_neighbors[j];
			if (pos[w] < min_p)
			{
				min_p = pos[w];
				best_parent = w;
			}
		}
		parent[u] = best_parent; // 0 означает, что мешок является корнем дерева

		// Поддерживаем хордальность рабочего графа для корректного сбора следующих мешков
		for (int x = 0; x < count; x++)
		{
			for (int y = x + 1; y < count; y++)
			{
				if (!has_edge(working_g, higher_neighbors[x], higher_neighbors[y]))
				{
					add_edge(working_g, higher_neighbors[x], higher_neighbors[y]);
				}
			}
		}
		free(higher_neighbors);
	}

	// ================= СТРОГИЙ АУДИТ МАТЕМАТИЧЕСКИХ СВОЙСТВ =================

	// === Шаг 2. Проверка Свойства 1: Все ли вершины исходного графа покрыты мешками? ===
	bool prop1 = true;
	for (int v = 1; v <= n; v++)
	{
		bool found = false;
		for (int b = 1; b <= n; b++)
		{
			for (int k = 0; k < bag_sizes[b]; k++)
			{
				if (bags[b][k] == v)
				{
					found = true;
					break;
				}
			}
			if (found)
				break;
		}
		if (!found)
		{
			// Точечная локализация ошибки
			fprintf(stderr, "   [Детальная ОШИБКА]: Свойство 1 нарушено! Вершина №%d не содержится ни в одном мешке дерева.\n", v);
			prop1 = false;
			break;
		}
	}

	// === Шаг 3. Проверка Свойства 2: Все ли исходные рёбра графа покрыты мешками? ===
	// (Для любого ребра (u, v) должен существовать мешок, содержащий И u, И v одновременно)
	bool prop2 = true;
	for (int u = 1; u <= n; u++)
	{
		for (int i = 0; i < g->adj[u].degree; i++)
		{
			int v = g->adj[u].neighbors[i];
			if (u < v) // Проверяем каждое ребро строго один раз
			{
				bool pair_found = false;
				for (int b = 1; b <= n; b++)
				{
					bool has_u = false, has_v = false;
					for (int k = 0; k < bag_sizes[b]; k++)
					{
						if (bags[b][k] == u)
							has_u = true;
						if (bags[b][k] == v)
							has_v = true;
					}
					if (has_u && has_v) // Нашли мешок, который покрыл ребро целиком
					{
						pair_found = true;
						break;
					}
				}
				if (!pair_found)
				{
					// Точечная локализация разорванного ребра
					fprintf(stderr, "   [Детальная ОШИБКА]: Свойство 2 нарушено! Ребро (%d, %d) потеряно. Нет мешка, вмещающего обе вершины.\n", u, v);
					prop2 = false;
					break;
				}
			}
		}
		if (!prop2)
			break;
	}

	// === Шаг 4. Проверка Свойства 3: Связность поддеревьев (Аксиома связности) ===
	// (Для любой вершины v множество мешков, содержащих v, должно образовывать связное поддерево)

	// Сначала строим стандартный список смежности для дерева декомпозиции на основе массива parent
	int *tree_deg = (int *)calloc(n + 1, sizeof(int));
	for (int u = 1; u <= n; u++)
	{
		if (parent[u] != 0)
		{
			tree_deg[u]++;
			tree_deg[parent[u]]++;
		}
	}
	int **tree_adj = (int **)malloc((n + 1) * sizeof(int *));
	for (int i = 1; i <= n; i++)
		tree_adj[i] = (int *)malloc(tree_deg[i] * sizeof(int));

	int *tree_curr_deg = (int *)calloc(n + 1, sizeof(int));
	for (int u = 1; u <= n; u++)
	{
		if (parent[u] != 0)
		{
			int p = parent[u];
			tree_adj[u][tree_curr_deg[u]++] = p;
			tree_adj[p][tree_curr_deg[p]++] = u;
		}
	}

	bool prop3 = true;
	bool *contains_v = (bool *)malloc((n + 1) * sizeof(bool));
	bool *visited = (bool *)malloc((n + 1) * sizeof(bool));
	int *queue = (int *)malloc((n + 1) * sizeof(int));

	// Проверяем связность мешков индивидуально для каждой вершины v
	for (int v = 1; v <= n; v++)
	{
		int total_bags_with_v = 0;
		int start_bag = -1;

		// Маркируем все мешки, в которых сидит вершина v
		for (int b = 1; b <= n; b++)
		{
			contains_v[b] = false;
			for (int k = 0; k < bag_sizes[b]; k++)
			{
				if (bags[b][k] == v)
				{
					contains_v[b] = true;
					total_bags_with_v++;
					start_bag = b; // Запоминаем стартовую точку для запуска BFS
					break;
				}
			}
		}

		// Если вершина присутствует меньше чем в двух мешках, проверять связность нет необходимости
		if (total_bags_with_v <= 1)
			continue;

		// Запускаем BFS внутри дерева декомпозиции, перемещаясь СТРОГО по мешкам, содержащим вершину v
		memset(visited, 0, (n + 1) * sizeof(bool));
		int head = 0, tail = 0;
		queue[tail++] = start_bag;
		visited[start_bag] = true;
		int visited_count = 1;

		while (head < tail)
		{
			int curr = queue[head++];
			for (int i = 0; i < tree_deg[curr]; i++)
			{
				int neighbor = tree_adj[curr][i];
				// Переходим к соседу только если он тоже содержит вершину v и еще не посещен в BFS
				if (contains_v[neighbor] && !visited[neighbor])
				{
					visited[neighbor] = true;
					queue[tail++] = neighbor;
					visited_count++;
				}
			}
		}

		// Если количество посещенных мешков меньше, чем общее число мешков с этой вершиной,
		// значит, граф мешков распался на изолированные части — связность нарушена!
		if (visited_count != total_bags_with_v)
		{
			fprintf(stderr, "   [Детальная ОШИБКА]: Свойство 3 нарушено! Информационная связность для вершины №%d разорвана.\n", v);
			prop3 = false;
			break;
		}
	}

	// Выводим структурированный аналитический отчёт валидации на консоль
	printf("  [Валидатор] Свойство 1 (Покрытие вершин): %s\n", prop1 ? "ОК (100%)" : "ОШИБКА!");
	printf("  [Валидатор] Свойство 2 (Покрытие рёбер):  %s\n", prop2 ? "ОК (100%)" : "ОШИБКА!");
	printf("  [Валидатор] Свойство 3 (Связность дерева): %s\n", prop3 ? "ОК (100%)" : "ОШИБКА!");

	bool is_valid = prop1 && prop2 && prop3;
	printf("  => РЕЗУЛЬТАТ: Древовидная декомпозиция %s!\n", is_valid ? "ПОЛНОСТЬЮ КОРРЕКТНА" : "НЕЛИКВИДНА");

	// === Тотальная очистка всей динамической памяти локальных структур ===
	free(pos);
	free(tree_deg);
	free(tree_curr_deg);
	free(contains_v);
	free(visited);
	free(queue);
	for (int i = 1; i <= n; i++)
	{
		free(bags[i]);
		free(tree_adj[i]);
	}
	free(bags);
	free(bag_sizes);
	free(parent);
	free(tree_adj);
	free_graph(working_g);

	return is_valid;
}