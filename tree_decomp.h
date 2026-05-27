#ifndef TREE_DECOMP_H
#define TREE_DECOMP_H

#include "graph.h"
#include <stdbool.h>

typedef struct
{
	int treewidth;
	int fill_in;
} OrderingMetrics;

// Расчёт метрик хордального дополнения
OrderingMetrics compute_chordal_completion(const Graph *g, const int *ordering);

// ФУНКЦИЯ ВАЛИДАЦИИ: Проверяет все 3 свойства декомпозиции и выводит отчёт
bool validate_tree_decomposition(const Graph *g, const int *ordering);

#endif // TREE_DECOMP_H