#ifndef OBJECTIVEFUNCTION_H
#define OBJECTIVEFUNCTION_H

#include <vector>
#include <map>
#include "Operation.h"

extern int m;
extern int o;
extern int t;
extern int c;

double objectiveFunction(const std::vector<std::vector<Operation>>& maquina, std::map<int, double> tempo_final, std::vector<Operation> vetOperacao, std::vector<vector<int>> controleOp);

#endif