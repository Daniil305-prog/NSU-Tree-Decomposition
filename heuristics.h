#ifndef HEURISTICS_H
#define HEURISTICS_H

#include "graph.h"

// Эвристика 1: Minimum Degree
int *min_degree_ordering(const Graph *g);

// Эвристика 2: Maximum Cardinality Search (MCS)
int *mcs_ordering(const Graph *g);

// Эвристика 3: Nested Dissection (Чистый BFS)
int *nested_dissection_ordering(const Graph *g);

// Эвристика 4: Minimum Fill-In
int *min_fill_in_ordering(const Graph *g);

// Эвристика 5: ГИБРИД (Nested Dissection на верхнем уровне + MCS на мелких подграфах)
int *hybrid_nd_mcs_ordering(const Graph *g);

#endif // HEURISTICS_H