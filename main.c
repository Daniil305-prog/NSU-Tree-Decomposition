#include "graph.h"
#include "heuristics.h"
#include "tree_decomp.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Генерирует сетку 10x10 (оставляем как запасной встроенный тест)
void create_grid_graph_file(const char *filename)
{
	FILE *f = fopen(filename, "w");
	if (!f)
		return;
	int rows = 10, cols = 10;
	int num_vertices = rows * cols;
	int num_edges = (rows - 1) * cols + (cols - 1) * rows;
	fprintf(f, "p tw %d %d\n", num_vertices, num_edges);
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			int v = r * cols + c + 1;
			if (r < rows - 1)
				fprintf(f, "%d %d\n", v, (r + 1) * cols + c + 1);
			if (c < cols - 1)
				fprintf(f, "%d %d\n", v, r * cols + (c + 1) + 1);
		}
	}
	fclose(f);
}

int main(int argc, char *argv[])
{
	// Включаем UTF-8 для консоли Windows
	system("chcp 65001 > nul");

	const char *filename = "test_graph.gr";
	bool is_custom_file = false;

	// Проверяем: передал ли пользователь файл в аргументах командной строки?
	if (argc > 1)
	{
		filename = argv[1];
		is_custom_file = true;
		printf("--- Режим: Работа с внешним графом из файла: %s ---\n", filename);
	}
	else
	{
		printf("--- Режим: Аргументы не переданы. Генерируем встроенный тест (сетка 10x10) ---\n");
		create_grid_graph_file(filename);
	}

	// Читаем граф (неважно, созданный или внешний)
	Graph *g = read_graph_dimacs(filename);
	if (!g)
	{
		printf("Ошибка чтения графа! Проверьте путь к файлу: %s\n", filename);
		return 1;
	}

	printf("Вершин: %d, Рёбер: %d\n\n", g->num_vertices, g->num_edges);

	clock_t start, end;
	double time_ms;

	// 1. Minimum Degree
	printf("=== Эвристика 1: Minimum Degree ===\n");
	start = clock();
	int *o1 = min_degree_ordering(g);
	end = clock();
	time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
	OrderingMetrics m1 = compute_chordal_completion(g, o1);
	printf("Ширина: %d, Fill-in: %d, Время: %.2f мс\n", m1.treewidth, m1.fill_in, time_ms);
	validate_tree_decomposition(g, o1);
	printf("\n");
	free(o1);

	// 2. MCS
	printf("=== Эвристика 2: Maximum Cardinality Search (MCS) ===\n");
	start = clock();
	int *o2 = mcs_ordering(g);
	end = clock();
	time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
	OrderingMetrics m2 = compute_chordal_completion(g, o2);
	printf("Ширина: %d, Fill-in: %d, Время: %.2f мс\n", m2.treewidth, m2.fill_in, time_ms);
	validate_tree_decomposition(g, o2);
	printf("\n");
	free(o2);

	// 3. Nested Dissection
	printf("=== Эвристика 3: Nested Dissection ===\n");
	start = clock();
	int *o3 = nested_dissection_ordering(g);
	end = clock();
	time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
	OrderingMetrics m3 = compute_chordal_completion(g, o3);
	printf("Ширина: %d, Fill-in: %d, Время: %.2f мс\n", m3.treewidth, m3.fill_in, time_ms);
	validate_tree_decomposition(g, o3);
	printf("\n");
	free(o3);

	// 4. Minimum Fill-In
	printf("=== Эвристика 4: Minimum Fill-In ===\n");
	start = clock();
	int *o4 = min_fill_in_ordering(g);
	end = clock();
	time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
	OrderingMetrics m4 = compute_chordal_completion(g, o4);
	printf("Ширина: %d, Fill-in: %d, Время: %.2f мс\n", m4.treewidth, m4.fill_in, time_ms);
	validate_tree_decomposition(g, o4);
	printf("\n");
	free(o4);

	// 5. ГИБРИД ND + MCS
	printf("=== Эвристика 5: Гибрид (ND + MCS) ===\n");
	start = clock();
	int *o5 = hybrid_nd_mcs_ordering(g);
	end = clock();
	time_ms = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
	OrderingMetrics m5 = compute_chordal_completion(g, o5);
	printf("Ширина: %d, Fill-in: %d, Время: %.2f мс\n", m5.treewidth, m5.fill_in, time_ms);
	validate_tree_decomposition(g, o5);
	printf("\n");
	free(o5);

	free_graph(g);
	return 0;
}