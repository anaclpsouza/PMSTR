#ifndef OBJECTIVEFUNCTION_H
#define OBJECTIVEFUNCTION_H
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <cstdlib>
#include "Operation.h"
#include <climits>

using namespace std;

static inline bool objectiveDebugEnabled()
{
    const char *env = std::getenv("PMSTR_DEBUG_LOCAL_SEARCH");
    return env != nullptr && env[0] != '\0' && env[0] != '0';
}

extern int m; // numero de maquinas
extern int o; // numero de operacoes
extern int t; // numero de conjuntos de ferramentas
extern int c; // capacidade do magazine

double objectiveFunction(const std::vector<std::vector<Operation>> &maquina,
                         const std::map<int, double> &tempo_final_base,
                         const std::vector<Operation> &vetOperacoes,
                         const std::map<int, std::map<int, int>> &controleOp_base,
                         std::vector<double> &tardiness_maq)
{
    if (maquina.empty())
    {
        if (objectiveDebugEnabled())
            std::cout << "[DEBUG] Lista de maquinas vazia." << std::endl;
        return 0.0;
    }

    if (vetOperacoes.size() != o)
    {
        return INT_MAX;
    }

    std::map<int, double> tempo_final = tempo_final_base;
    std::map<int, std::map<int, int>> controleOp = controleOp_base;
    tardiness_maq.assign(m, 0.0);

    double wd = 1.0;  // Peso do atraso
    double ws = 1.0;  // Peso da troca
    double tau = 1.0; // Tempo de troca (1 hora)

    int setups = 0;
    double tardiness = 0.0;
    vector<double> tempo(m, 0.0);
    tardiness_maq.resize(m, 0.0);

    vector<int> u(m, 0); // quantidade de ferramentas atualmente carregadas
    vector<vector<int>> carregados(m, vector<int>(t, 0));
    vector<vector<vector<int>>> magazine(m, vector<vector<int>>(t, vector<int>())); // magazine de presença de ferramentas
    vector<vector<vector<int>>> prioridades(m, vector<vector<int>>(t, vector<int>()));

    vector<int> setsSize(t);

    for (int i = 0; i < m; ++i)
    {
        for (int j = 0; j < maquina[i].size(); j++)
        {
            setsSize[maquina[i][j].toolSetId] = maquina[i][j].toolSetSize;
        }
    }

    for (int i = 0; i < m; ++i)
    {
        for (int j = 0; j < t; j++)
        {
            for (int k = 0; k < maquina[i].size(); k++)
            {
                magazine[i][j].push_back(0);
                prioridades[i][j].push_back(0);
            }
        }
    }

    // Para cada máquina, para cada ferramenta usada pela operacao em cada estágio
    for (int i = 0; i < m; ++i) // máquina
    {
        for (int j = 0; j < maquina[i].size(); ++j) // estágio do magazine
        {
            magazine[i][maquina[i][j].toolSetId][j] = 1;
        }
    }

    for (int k = 0; k < m; ++k) // máquina
    {
        for (unsigned i = 0; i < t; ++i) // ferramenta
        {
            for (unsigned j = 0; j < maquina[k].size(); ++j) // operação
            {
                if (magazine[k][i][j] == 1) // se o estágio atual precisa daquela ferramenta, prioridade zero
                    prioridades[k][i][j] = 0;
                else
                {
                    int proxima = 0;
                    bool usa = false;
                    for (unsigned l = j + 1; l < maquina[k].size(); ++l)
                    {
                        ++proxima;
                        if (magazine[k][i][l] == 1)
                        {
                            usa = true;
                            break;
                        }
                    }
                    if (usa)
                        prioridades[k][i][j] = proxima;
                    else
                        prioridades[k][i][j] = -1;
                }
            }
        }
    }

    for (int j = 0; j < m; ++j) // maquina
    {
        for (int i = 0; i < maquina[j].size(); i++) // operacao
        {
            if (carregados[j][maquina[j][i].toolSetId] == 0 && u[j] + maquina[j][i].toolSetSize < c)
            {
                u[j] += maquina[j][i].toolSetSize;
                carregados[j][maquina[j][i].toolSetId] = 1;
            }
        }
    }

    int max_op = 0;
    for (int i = 0; i < maquina.size(); ++i)
    {
        if (maquina[i].size() > max_op)
            max_op = maquina[i].size();
    }
    for (int j = 0; j < max_op; ++j) // operacao
    {
        for (int i = 0; i < maquina.size(); i++) // máquinas
        {
            if (j >= maquina[i].size())
                continue;

            int idJob = maquina[i][j].idJob;
            int idOp = maquina[i][j].idOp;
            bool troca = false;

            if (idOp > 1)
            {
                if (!controleOp[idJob][idOp - 1] == 1)
                {
                    return INT_MAX;
                }
            }
            controleOp[idJob][idOp] = 1;

            if (carregados[i][maquina[i][j].toolSetId] == 0)
            {
                u[i] += maquina[i][j].toolSetSize;
                carregados[i][maquina[i][j].toolSetId] = 1; // carrega a ferramenta necessária para a operação atual
                troca = true;

                while (u[i] > c)
                {
                    int maior = 0;
                    int pMaior = -1;

                    for (unsigned k = 0; k < t; ++k) // ferramentas
                    {
                        if (magazine[i][k][j] != 1) // matriz de ferramentas
                        {
                            if ((carregados[i][k] == 1) && (prioridades[i][k][j] == -1))
                            {
                                pMaior = k;
                                break;
                            }
                            else
                            {
                                if ((prioridades[i][k][j] > maior) && carregados[i][k] == 1)
                                {
                                    maior = prioridades[i][k][j];
                                    pMaior = k;
                                }
                            }
                        }
                    }
                    if (pMaior == -1)
                        return 0;
                    carregados[i][pMaior] = 0;
                    u[i] -= setsSize[pMaior];
                }
            }
            if (troca)
            {
                ++setups;
            }
            double tempoMin = 0.0;
            if (tempo_final[idJob] > 0)
            {
                tempoMin = tempo_final[idJob];
            }

            tempo[i] = std::max({tempo[i], tempoMin, (double)maquina[i][j].releaseTime});

            tempo[i] += maquina[i][j].processingTime; // processa a operação (adiciona o processing time)

            if (troca)
            {
                tempo[i] += tau;
            }

            tempo_final[idJob] = tempo[i];

            if (tempo[i] > maquina[i][j].dueDate)
            {
                tardiness_maq[i] = tempo[i] - maquina[i][j].dueDate;
                tardiness += tardiness_maq[i];
            }
        }
    }

    if (objectiveDebugEnabled())
        std::cout << "[DEBUG] Tardiness final: " << tardiness << std::endl;
    double resultado = (wd * tardiness) + (ws * tau * setups);
    if (objectiveDebugEnabled())
        std::cout << "[DEBUG] Total setups=" << setups << std::endl;
    return resultado;
}
#endif