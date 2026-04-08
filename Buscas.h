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
std::random_device rd;
std::mt19937 rng(rd());

static inline bool buscasDebugEnabled()
{
    static const bool enabled = []()
    {
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

long re_insertion(std::vector<std::vector<Operation>> &maquina,
                  std::map<int, double> &tempo_final,
                  std::vector<Operation> &vetOperacoes,
                  std::map<int, std::map<int, int>> &controleOp,
                  std::vector<double> &tardiness_maq)
{
    long r0 = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_maq);
    long resultadoAtual = r0;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][IS] Objetivo inicial: " << r0 << std::endl;
        for (size_t idx = 0; idx < maquina.size(); ++idx)
        {
            std::cout << "[DEBUG][IS] Maquina " << idx << " possui " << maquina[idx].size() << " operacoes" << std::endl;
        }
    }

    int numMaquinas = maquina.size();

    // Máquina com maior atraso primeiro
    std::vector<int> indicesMaquinas(numMaquinas);
    std::iota(indicesMaquinas.begin(), indicesMaquinas.end(), 0);
    std::sort(indicesMaquinas.begin(), indicesMaquinas.end(), [&](int a, int b)
              { return tardiness_maq[a] > tardiness_maq[b]; });

    std::vector<double> tardiness_teste = tardiness_maq;

    for (int m : indicesMaquinas)
    {
        if (maquina[m].size() < 2)
            continue;

        if (buscasDebugEnabled())
        {
            std::cout << "[DEBUG][IS] Explorando maquina " << m
                      << " (tardiness=" << tardiness_maq[m]
                      << ", operacoes=" << maquina[m].size() << ")" << std::endl;
        }

        int numOps = maquina[m].size();
        std::vector<int> ordemPosicoes(numOps);
        std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
        std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

        for (int i : ordemPosicoes)
        {
            for (int j = 0; j < numOps; ++j)
            {
                if (i == j)
                    continue;

                Operation opCopia = maquina[m][i];
                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][IS] Tentando mover operacao id=" << opCopia.id
                              << " (job=" << opCopia.idJob << ", op=" << opCopia.idOp
                              << ") da posicao " << i << " para " << j
                              << " na maquina " << m << std::endl;
                }

                maquina[m].erase(maquina[m].begin() + i);
                maquina[m].insert(maquina[m].begin() + j, opCopia);

                long resultadoNovo = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_teste);

                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][IS] Resultado candidato: " << resultadoNovo
                              << " | atual: " << resultadoAtual << std::endl;
                }

                // First Improvement: Se melhorou, encerra a busca local e retorna S'
                if (resultadoNovo < resultadoAtual)
                {
                    if (buscasDebugEnabled())
                    {
                        std::cout << "[DEBUG][IS] Melhoria encontrada! Novo objetivo=" << resultadoNovo
                                  << " ganho=" << (r0 - resultadoNovo) << std::endl;
                    }
                    
                    tardiness_maq = tardiness_teste;
                    return resultadoNovo;
                }

                // se não melhorou reverte
                maquina[m].erase(maquina[m].begin() + j);
                maquina[m].insert(maquina[m].begin() + i, opCopia);

                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][IS] Movimento rejeitado e revertido." << std::endl;
                }
            }
        }
    }

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][IS] Nenhuma melhoria encontrada. Retornando objetivo inicial." << std::endl;
    }
    return r0;
}

long insertion_im(std::vector<std::vector<Operation>> &maquina,
                  std::map<int, double> &tempo_final,
                  std::vector<Operation> &vetOperacoes,
                  std::map<int, std::map<int, int>> &controleOp,
                  std::vector<double> &tardiness_maq)
{
    long r0 = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_maq);
    long resultadoAtual = r0;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ISIM] Objetivo inicial: " << r0 << std::endl;
    }

    int numMaquinas = maquina.size();
    std::vector<int> indicesMaquinas(numMaquinas);
    std::iota(indicesMaquinas.begin(), indicesMaquinas.end(), 0);

    std::shuffle(indicesMaquinas.begin(), indicesMaquinas.end(), rng);

    std::stable_sort(indicesMaquinas.begin(), indicesMaquinas.end(), [&](int a, int b)
                     { return tardiness_maq[a] < tardiness_maq[b]; });

    int maq_atrasada = indicesMaquinas.back();
    indicesMaquinas.pop_back(); // Remove a origem da lista de destinos

    std::vector<double> tardiness_teste = tardiness_maq;

    for (size_t i = 0; i < maquina[maq_atrasada].size(); i++)
    {
        Operation op = maquina[maq_atrasada][i];

        maquina[maq_atrasada].erase(maquina[maq_atrasada].begin() + i);

        for (int m : indicesMaquinas)
        {
            maquina[m].push_back(op);
            int posAtual = maquina[m].size() - 1;

            int numOpsDestino = maquina[m].size();
            std::vector<int> ordemPosicoes(numOpsDestino);
            std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
            std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

            for (int j : ordemPosicoes)
            {
                Operation opCopia = maquina[m][posAtual];
                maquina[m].erase(maquina[m].begin() + posAtual);
                maquina[m].insert(maquina[m].begin() + j, opCopia);

                long resultadoNovo = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_teste);

                if (resultadoNovo < resultadoAtual)
                {
                    if (buscasDebugEnabled())
                    {
                        std::cout << "[DEBUG][ISIM] Melhoria encontrada! Novo objetivo=" << resultadoNovo
                                  << " ganho=" << (r0 - resultadoNovo) << std::endl;
                    }

                    tardiness_maq = tardiness_teste; 
                    return resultadoNovo;
                }

                maquina[m].erase(maquina[m].begin() + j);
                maquina[m].insert(maquina[m].begin() + posAtual, opCopia);
            }

            maquina[m].pop_back();
        }

        maquina[maq_atrasada].insert(maquina[maq_atrasada].begin() + i, op);
    }

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ISIM] Nenhuma melhoria encontrada. Retornando objetivo inicial=" << r0 << std::endl;
    }
    return r0;
}

long two_swap(std::vector<std::vector<Operation>> &maquina,
              std::map<int, double> &tempo_final,
              std::vector<Operation> &vetOperacoes,
              std::map<int, std::map<int, int>> &controleOp,
              std::vector<double> &tardiness_maq)
{
    long r0 = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_maq);
    long resultadoAtual = r0;

    int numMaquinas = maquina.size();

    std::vector<int> indicesMaquinas(numMaquinas);
    std::iota(indicesMaquinas.begin(), indicesMaquinas.end(), 0);
    std::sort(indicesMaquinas.begin(), indicesMaquinas.end(), [&](int a, int b)
              { return tardiness_maq[a] > tardiness_maq[b]; });

    std::vector<double> tardiness_teste = tardiness_maq;

    for (int m : indicesMaquinas)
    {
        size_t n = maquina[m].size();
        if (n < 2)
            continue;

        for (size_t i = 0; i < n - 1; i++)
        {
            for (size_t j = i + 1; j < n; j++)
            {
                std::swap(maquina[m][i], maquina[m][j]);

                long novoResultado = objectiveFunction(maquina, tempo_final, vetOperacoes, controleOp, tardiness_teste);

                if (novoResultado < resultadoAtual)
                {
                    if (buscasDebugEnabled())
                    {
                        std::cout << "[DEBUG] Melhoria na Maquina " << m << ": " << novoResultado << std::endl;
                    }
                    tardiness_maq = tardiness_teste;
                    return novoResultado;
                }

                std::swap(maquina[m][i], maquina[m][j]);
            }
        }
    }

    return r0;
}
#endif