#ifndef BUSCAS_H
#define BUSCAS_H

#include <chrono>
#include <utility>
#include <vector>
#include "Operation.h"

extern std::chrono::high_resolution_clock::time_point inicioExecucao;

double perturbacao_enviesada(std::vector<std::vector<Operation>> &solucaoMaquinas,
                             const std::vector<Operation> &operacoes,
                             std::vector<double> &atrasoPorMaquina,
                             const std::vector<std::pair<int, Operation>> &operacoesEmEspera,
                             int qtdPerturbacoes);

double re_insertion(std::vector<std::vector<Operation>> &solucaoMaquinas,
                    const std::vector<Operation> &operacoes,
                    std::vector<double> &atrasoPorMaquina);

double insertion_im(std::vector<std::vector<Operation>> &solucaoMaquinas,
                    const std::vector<Operation> &operacoes,
                    std::vector<double> &atrasoPorMaquina);

double two_swap(std::vector<std::vector<Operation>> &solucaoMaquinas,
                const std::vector<Operation> &operacoes,
                std::vector<double> &atrasoPorMaquina);

double ILS(int usarReInsertion,
           int usarInsertionIm,
           int usarTwoSwap,
           int limiteIteracoesSemMelhoria,
           double percentualPerturbacao,
           int limiteTempoHoras,
           std::vector<std::vector<Operation>> &solucaoMaquinas,
           const std::vector<Operation> &operacoes,
           std::vector<double> &atrasoPorMaquina,
           int totalOperacoes);

#endif
