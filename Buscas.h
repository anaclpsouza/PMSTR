#ifndef BUSCAS_H
#define BUSCAS_H
#include <ctime>
#include <ratio>
#include <string>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <random>
#include <utility>
#include <vector>
#include <iostream>
#include <limits>
#include <time.h>
#include <climits>
#include "Operation.h"
#include "ObjectiveFunction.h"

using namespace std;

static inline bool buscasDebugEnabled()
{
    static const bool enabled = []() {
        const char *env = std::getenv("PMSTR_DEBUG_LOCAL_SEARCH");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return enabled;
}

static inline void buscasDebugLog(const std::string &msg)
{
    if (buscasDebugEnabled())
        std::cout << msg << std::endl;
}


long insertion(std::vector<std::vector<Operation>> &maquina, 
              std::map<int, double> &tempo_final, 
              std::vector<Operation> &vetOperacoes, 
              std::map<int, std::map<int, int>> &controleOp, 
              std::vector<double> &tardiness_maq) 
{
    long r0 = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_maq);
    long resultadoAtual = r0;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][LS] Objetivo inicial: " << r0 << std::endl;
        for (size_t idx = 0; idx < maquina.size(); ++idx)
        {
            std::cout << "[DEBUG][LS] Maquina " << idx << " possui " << maquina[idx].size() << " operacoes" << std::endl;
        }
    }
    
    int numMaquinas = maquina.size();
    
    // Máquina com maior atraso primeiro
    std::vector<int> indicesMaquinas(numMaquinas);
    std::iota(indicesMaquinas.begin(), indicesMaquinas.end(), 0);
    std::sort(indicesMaquinas.begin(), indicesMaquinas.end(), [&](int a, int b) {
        return tardiness_maq[a] > tardiness_maq[b];
    });

    std::random_device rd;
    std::mt19937 rng(rd());

    for (int m : indicesMaquinas) {
        if (maquina[m].size() < 2) continue; 

        if (buscasDebugEnabled())
        {
            std::cout << "[DEBUG][LS] Explorando maquina " << m
                      << " (tardiness=" << tardiness_maq[m]
                      << ", operacoes=" << maquina[m].size() << ")" << std::endl;
        }

        int numOps = maquina[m].size();
        std::vector<int> ordemPosicoes(numOps);
        std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
        std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

        for (int i : ordemPosicoes) {
            for (int j = 0; j < numOps; ++j) {
                if (i == j) continue;

                Operation opCopia = maquina[m][i];
                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][LS] Tentando mover operacao id=" << opCopia.id
                              << " (job=" << opCopia.idJob << ", op=" << opCopia.idOp
                              << ") da posicao " << i << " para " << j
                              << " na maquina " << m << std::endl;
                }

                maquina[m].erase(maquina[m].begin() + i);
                maquina[m].insert(maquina[m].begin() + j, opCopia);

                long resultadoNovo = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_maq);

                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][LS] Resultado candidato: " << resultadoNovo
                              << " | atual: " << resultadoAtual << std::endl;
                }

                // First Improvement: Se melhorou, encerra a busca local e retorna S'
                if (resultadoNovo < resultadoAtual) {
                    if (buscasDebugEnabled())
                    {
                        std::cout << "[DEBUG][LS] Melhoria encontrada! Novo objetivo=" << resultadoNovo
                                  << " ganho=" << (r0 - resultadoNovo) << std::endl;
                    }
                    return r0 - resultadoNovo; // Retorna o ganho e sai da função
                }

               // se não melhorou reverte
                maquina[m].erase(maquina[m].begin() + j);
                maquina[m].insert(maquina[m].begin() + i, opCopia);

                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][LS] Movimento rejeitado e revertido." << std::endl;
                }
            }
        }
    }

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][LS] Nenhuma melhoria encontrada. Retornando 0." << std::endl;
    }
    return 0; 
}
#endif