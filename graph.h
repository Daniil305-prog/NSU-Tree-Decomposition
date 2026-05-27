#ifndef GRAPH_H
#define GRAPH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

// Структура для списка соседей конкретной вершины
typedef struct {
    int *neighbors;   // Динамический массив смежных вершин
    int degree;       // Текущая степень вершины (сколько соседей записано)
    int capacity;     // Выделенный размер массива в памяти
} AdjList;

// Основная структура графа
typedef struct {
    int num_vertices; // Количество вершин
    int num_edges;    // Количество рёбер
    AdjList *adj;     // Массив списков смежности (размер num_vertices + 1)
} Graph;

// Прототипы функций
Graph* create_graph(int num_vertices);
void add_edge(Graph *g, int u, int v);
bool has_edge(const Graph *g, int u, int v);
Graph* read_graph_dimacs(const char *filename);
void free_graph(Graph *g);

#endif // GRAPH_H