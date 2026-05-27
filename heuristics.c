#include "heuristics.h"
#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

// Предварительные объявления вспомогательных функций
static void nd_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx);
static void hybrid_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx);

// Клонирование графа
static Graph *clone_graph(const Graph *orig)
{
	Graph *copy = create_graph(orig->num_vertices);
	if (!copy)
		return NULL;
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

// 1. Minimum Degree
int *min_degree_ordering(const Graph *g)
{
	Graph *working_g = clone_graph(g);
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *eliminated = (bool *)calloc(n + 1, sizeof(bool));

	for (int step = 1; step <= n; step++)
	{
		int min_deg = -1, best_v = -1;
		for (int v = 1; v <= n; v++)
		{
			if (!eliminated[v])
			{
				int active_degree = 0;
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

		ordering[step] = best_v;
		eliminated[best_v] = true;

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

// 2. Maximum Cardinality Search (MCS)
int *mcs_ordering(const Graph *g)
{
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *visited = (bool *)calloc(n + 1, sizeof(bool));
	int *weight = (int *)calloc(n + 1, sizeof(int));

	for (int step = n; step >= 1; step--)
	{
		int max_weight = -1, best_v = -1;
		for (int v = 1; v <= n; v++)
		{
			if (!visited[v] && weight[v] > max_weight)
			{
				max_weight = weight[v];
				best_v = v;
			}
		}
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

		ordering[step] = best_v;
		visited[best_v] = true;

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

// Локальный запуск MCS внутри выделенного подграфа для Гибрида
static void local_mcs_for_hybrid(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx)
{
	bool *sub_mask = (bool *)calloc(g->num_vertices + 1, sizeof(bool));
	for (int i = 0; i < sub_count; i++)
		sub_mask[sub_vertices[i]] = true;

	int *weight = (int *)calloc(g->num_vertices + 1, sizeof(int));
	bool *local_visited = (bool *)calloc(g->num_vertices + 1, sizeof(bool));

	for (int step = 0; step < sub_count; step++)
	{
		int max_weight = -1, best_v = -1;
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

		ordering[*current_idx] = best_v;
		(*current_idx)--;
		local_visited[best_v] = true;
		global_placed[best_v] = true;

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

// 3. Nested Dissection
int *nested_dissection_ordering(const Graph *g)
{
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *global_placed = (bool *)calloc(n + 1, sizeof(bool));
	int *all_vertices = (int *)malloc(n * sizeof(int));
	for (int i = 0; i < n; i++)
		all_vertices[i] = i + 1;

	int current_idx = n;
	nd_helper(g, all_vertices, n, global_placed, ordering, &current_idx);

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

static void nd_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx)
{
	if (sub_count == 0)
		return;
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
	nd_helper(g, part_B, b_count, global_placed, ordering, current_idx);
	nd_helper(g, part_A, a_count, global_placed, ordering, current_idx);
	free(part_A);
	free(part_B);
}

// 4. Minimum Fill-In
int *min_fill_in_ordering(const Graph *g)
{
	Graph *working_g = clone_graph(g);
	int n = g->num_vertices;
	int *ordering = (int *)malloc((n + 1) * sizeof(int));
	bool *eliminated = (bool *)calloc(n + 1, sizeof(bool));

	for (int step = 1; step <= n; step++)
	{
		int min_fill = -1, best_v = -1;
		for (int v = 1; v <= n; v++)
		{
			if (!eliminated[v])
			{
				int active_count = 0;
				for (int i = 0; i < working_g->adj[v].degree; i++)
				{
					if (!eliminated[working_g->adj[v].neighbors[i]])
						active_count++;
				}
				int local_fill = 0;
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
					for (int i = 0; i < tc; i++)
					{
						for (int j = i + 1; j < tc; j++)
						{
							if (!has_edge(working_g, temp_neigh[i], temp_neigh[j]))
								local_fill++;
						}
					}
					free(temp_neigh);
				}
				if (min_fill == -1 || local_fill < min_fill)
				{
					min_fill = local_fill;
					best_v = v;
				}
			}
		}

		ordering[step] = best_v;
		eliminated[best_v] = true;

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

// 5. ГИБРИД (ND + MCS)
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

static void hybrid_helper(const Graph *g, int *sub_vertices, int sub_count, bool *global_placed, int *ordering, int *current_idx)
{
	if (sub_count == 0)
		return;

	// Порог переключения гибрида: если в куске осталось меньше 25 вершин,
	// прекращаем рекурсию ND и отдаём кусок на растерзание точному локальному MCS!
	if (sub_count <= 25)
	{
		local_mcs_for_hybrid(g, sub_vertices, sub_count, global_placed, ordering, current_idx);
		return;
	}

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
	hybrid_helper(g, part_B, b_count, global_placed, ordering, current_idx);
	hybrid_helper(g, part_A, a_count, global_placed, ordering, current_idx);
	free(part_A);
	free(part_B);
}