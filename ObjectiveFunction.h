#ifndef OBJECTIVEFUNCTION_H
#define OBJECTIVEFUNCTION_H

#include "Operation.h"
#include <utility>
#include <vector>

extern int numMaquinas;
extern int numOperacoes;
extern int numConjuntosFerramentas;
extern int capacidadeMagazine;

struct EstatisticasKTNS
{
    long long ktnsAntigo = 0;
    long long ktnsNovo = 0;
    long long empates = 0;
};

extern EstatisticasKTNS estatisticasKtns;
void resetarEstatisticasKtns();

double objectiveFunction(const std::vector<std::vector<Operation>> &solucaoMaquinas,
                         const std::vector<Operation> &operacoes,
                         std::vector<double> &atrasoPorMaquina,
                         std::vector<std::pair<int, Operation>> *operacoesEmEspera = nullptr);

#endif