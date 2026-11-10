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
#include <climits>

#include "Operation.h"
#include "ObjectiveFunction.h"
#include "Buscas.h"

using namespace std;
std::random_device rd;
std::mt19937 rng(rd());
using namespace std::chrono;
high_resolution_clock::time_point t1;
high_resolution_clock::time_point t2;
std::chrono::high_resolution_clock::duration tempo_execucao;
static std::chrono::high_resolution_clock::duration limite_tempo_execucao = std::chrono::hours(2);

static inline bool tempoLimiteAtingido()
{
    t2 = high_resolution_clock::now();
    tempo_execucao = t2 - t1;
    return tempo_execucao >= limite_tempo_execucao;
}

static inline bool buscasDebugEnabled()
{
    static const bool enabled = []()
    {
        const char *env = std::getenv("PMSTR_DEBUG_LOCAL_SEARCH");
        return env != nullptr && env[0] != '\0' && env[0] != '0';
    }();
    return enabled;
}

double ILS(int usarReInsertion,
           int usarInsertionIm,
           int usarTwoSwap,
           int limiteIteracoesSemMelhoria,
           double percentualPerturbacao,
           int limiteTempoHoras,
           std::vector<std::vector<Operation>> &maquina,
           std::vector<Operation> &vetOperacoes,
           std::map<int, std::map<int, int>> &controleOp,
           std::vector<double> &tardiness_maq,
           int totalOperacoes)
{
    auto executarBusca = [&](int tipo)
    {
        if (tipo == 0)
            return re_insertion(maquina, vetOperacoes, controleOp, tardiness_maq);
        if (tipo == 1)
            return insertion_im(maquina, vetOperacoes, controleOp, tardiness_maq);
        return two_swap(maquina, vetOperacoes, controleOp, tardiness_maq);
    };

    limite_tempo_execucao = std::chrono::hours(limiteTempoHoras);

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Iniciando ILS com " << maquina.size()
                  << " maquinas e " << totalOperacoes << " operacoes" << std::endl;
    }

    // Intensificacao inicial
    double s = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_maq);

    if (usarReInsertion == 1 && !tempoLimiteAtingido())
        s = executarBusca(0);
    if (usarInsertionIm == 1 && !tempoLimiteAtingido())
        s = executarBusca(1);
    if (usarTwoSwap == 1 && !tempoLimiteAtingido())
        s = executarBusca(2);

    double melhor = s;
    std::vector<std::vector<Operation>> melhorSolucao = maquina;
    std::vector<double> melhorTardiness = tardiness_maq;
    std::vector<std::vector<Operation>> solucaoBase = maquina;
    std::vector<double> tardinessBase = tardiness_maq;

    int iteracoesExecutadas = 0;
    int iteracoesSemMelhoriaConsecutivas = 0;
    bool parouPorTempo = false;

    const int quantidadePerturbacoes = std::max(
        1,
        static_cast<int>(std::ceil(totalOperacoes * percentualPerturbacao)));

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Solucao inicial intensificada=" << s
                  << " | perturbacao=" << quantidadePerturbacoes
                  << " | limite sem melhoria=" << limiteIteracoesSemMelhoria
                  << std::endl;
    }

    // Iteracoes consecutivas sem melhoria global.
    // Quando uma nova melhor solucao e encontrada, o contador volta para zero.
    while (iteracoesSemMelhoriaConsecutivas < limiteIteracoesSemMelhoria)
    {
        if (tempoLimiteAtingido())
        {
            parouPorTempo = true;
            break;
        }

        ++iteracoesExecutadas;

        maquina = solucaoBase;
        tardiness_maq = tardinessBase;

        std::vector<std::pair<int, Operation>> ops_emEspera;
        std::vector<double> tardiness_temp;
        objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_temp, nullptr, &ops_emEspera);

        // Perturbacao enviesada
        double sAtual = pertubacao_enviesada(maquina,
                                             vetOperacoes,
                                             controleOp,
                                             tardiness_maq,
                                             ops_emEspera,
                                             quantidadePerturbacoes);
        double sCandidato = sAtual;

        // VND
        int qual = 0;
        while (qual < 3)
        {
            if (tempoLimiteAtingido())
            {
                parouPorTempo = true;
                break;
            }

            if ((qual == 0 && usarReInsertion == 0) ||
                (qual == 1 && usarInsertionIm == 0) ||
                (qual == 2 && usarTwoSwap == 0))
            {
                ++qual;
                continue;
            }

            sCandidato = executarBusca(qual);

            if (sCandidato < sAtual)
            {
                sAtual = sCandidato;
                qual = 0;
            }
            else
            {
                ++qual;
            }
        }

        if (sAtual < melhor)
        {
            melhor = sAtual;
            melhorSolucao = maquina;
            melhorTardiness = tardiness_maq;
            iteracoesSemMelhoriaConsecutivas = 0;
        }
        else
        {
            ++iteracoesSemMelhoriaConsecutivas;
        }

        const bool aceitaComoBase = (sAtual < INT_MAX) && (sAtual <= (melhor * 1.10));
        if (aceitaComoBase)
        {
            solucaoBase = maquina;
            tardinessBase = tardiness_maq;
            s = sAtual;
        }

        if (parouPorTempo)
            break;
    }

    maquina = melhorSolucao;
    tardiness_maq = melhorTardiness;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Encerrando ILS com melhor=" << melhor
                  << " | iteracoes=" << iteracoesExecutadas
                  << " | sem melhoria finais="
                  << iteracoesSemMelhoriaConsecutivas << std::endl;
    }

    return melhor;
}

double pertubacao_enviesada(std::vector<std::vector<Operation>> &maquina,
                            std::vector<Operation> &vetOperacoes,
                            std::map<int, std::map<int, int>> &controleOp,
                            std::vector<double> &tardiness_maq,
                            const std::vector<std::pair<int, Operation>> &ops_emEspera,
                            int num_ops)
{
    int numMaquinas = maquina.size();
    std::vector<std::pair<int, Operation>> selecionadas;

    // pegando as operações que causaram espera
    for (const auto &op : ops_emEspera)
    {
        if (selecionadas.size() >= (size_t)num_ops)
            break;
        selecionadas.push_back(op);
    }

    // completando com aleatórias se necessário
    while (selecionadas.size() < (size_t)num_ops)
    {
        int m_rand = std::uniform_int_distribution<>(0, numMaquinas - 1)(rng);
        if (maquina[m_rand].empty())
            continue;
        int pos_rand = std::uniform_int_distribution<>(0, maquina[m_rand].size() - 1)(rng);

        selecionadas.push_back(std::make_pair(m_rand, maquina[m_rand][pos_rand]));
    }

    std::vector<double> tardiness_teste = tardiness_maq;

    // perturbação de fato
    for (const auto &item : selecionadas)
    {
        if (tempoLimiteAtingido())
            break;

        int maqOrigem = item.first;
        Operation op = item.second;

        // remove a operação da máquina de origem
        bool removido = false;
        int posOrigem = -1;
        for (auto it = maquina[maqOrigem].begin(); it != maquina[maqOrigem].end(); ++it)
        {
            if (it->idJob == op.idJob && it->idOp == op.idOp)
            {
                posOrigem = std::distance(maquina[maqOrigem].begin(), it);
                maquina[maqOrigem].erase(it);
                removido = true;
                break;
            }
        }
        if (!removido)
            continue;

        // escolhe máquina de destino aleatória e DIFERENTE da origem
        int maqDestino = maqOrigem;
        if (numMaquinas > 1)
        {
            while (maqDestino == maqOrigem)
            {
                maqDestino = std::uniform_int_distribution<>(0, numMaquinas - 1)(rng);
            }
        }

        // testa todas as posições possíveis na máquina de destino
        int melhorPosicao = -1;
        double melhorSol = std::numeric_limits<double>::max();

        int numOpsDestino = maquina[maqDestino].size();

        // embaralhando a ordem de teste
        std::vector<int> ordemPosicoes(numOpsDestino + 1); // +1 pq pode inserir no final
        std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
        std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

        // testando
        for (int pos : ordemPosicoes)
        {
            if (tempoLimiteAtingido())
                break;

            maquina[maqDestino].insert(maquina[maqDestino].begin() + pos, op);

            double solAtual = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_teste);

            if (solAtual < melhorSol && solAtual < INT_MAX)
            {
                melhorSol = solAtual;
                melhorPosicao = pos;
            }

            maquina[maqDestino].erase(maquina[maqDestino].begin() + pos);
        }

        // insere real
        if (melhorPosicao != -1)
        {
            maquina[maqDestino].insert(maquina[maqDestino].begin() + melhorPosicao, op);
        }
        else
        {
            // caso bizarro onde nada melhora, reverte p essa operação; conversar com o leo dps sobre
            maquina[maqOrigem].insert(maquina[maqOrigem].begin() + posOrigem, op);
        }
    }

    return objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_maq);
}

double re_insertion(std::vector<std::vector<Operation>> &maquina,
                    std::vector<Operation> &vetOperacoes,
                    std::map<int, std::map<int, int>> &controleOp,
                    std::vector<double> &tardiness_maq)
{
    double r0 = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_maq);
    double resultadoAtual = r0;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][IS] Objetivo inicial: " << r0 << std::endl;
        for (size_t idx = 0; idx < maquina.size(); ++idx)
        {
            std::cout << "[DEBUG][IS] Maquina " << idx << " possui " << maquina[idx].size() << " operacoes" << std::endl;
        }
    }

    int numMaquinas = maquina.size();

    // Máquina com maior atraso primeiro (desempate aleatorio)
    std::vector<int> indicesMaquinas(numMaquinas);
    std::iota(indicesMaquinas.begin(), indicesMaquinas.end(), 0);
    std::shuffle(indicesMaquinas.begin(), indicesMaquinas.end(), rng);
    std::stable_sort(indicesMaquinas.begin(), indicesMaquinas.end(), [&](int a, int b)
                     { return tardiness_maq[a] > tardiness_maq[b]; });

    std::vector<double> tardiness_teste = tardiness_maq;

    for (int m : indicesMaquinas)
    {
        if (tempoLimiteAtingido())
            return r0;

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
            if (tempoLimiteAtingido())
                return r0;

            for (int j = 0; j < numOps; ++j)
            {
                if (i == j)
                    continue;

                if (tempoLimiteAtingido())
                    return r0;

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

                double resultadoNovo = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_teste);

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

double insertion_im(std::vector<std::vector<Operation>> &maquina,
                    std::vector<Operation> &vetOperacoes,
                    std::map<int, std::map<int, int>> &controleOp,
                    std::vector<double> &tardiness_maq)
{
    double r0 = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_maq);
    double resultadoAtual = r0;

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

    // Maquinas de origem: da mais atrasada para a menos atrasada
    std::vector<int> maquinasOrigem(indicesMaquinas.rbegin(), indicesMaquinas.rend());
    std::vector<double> tardiness_teste = tardiness_maq;

    for (int maq_origem : maquinasOrigem)
    {
        if (tempoLimiteAtingido())
            return r0;

        for (size_t i = 0; i < maquina[maq_origem].size(); i++)
        {
            if (tempoLimiteAtingido())
                return r0;

            Operation op = maquina[maq_origem][i];
            maquina[maq_origem].erase(maquina[maq_origem].begin() + i);

            for (int m : indicesMaquinas)
            {
                if (m == maq_origem)
                    continue;

                if (tempoLimiteAtingido())
                {
                    maquina[maq_origem].insert(maquina[maq_origem].begin() + i, op);
                    return r0;
                }

                int numOpsDestino = maquina[m].size() + 1;
                std::vector<int> ordemPosicoes(numOpsDestino);
                std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
                std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

                for (int j : ordemPosicoes)
                {
                    if (tempoLimiteAtingido())
                    {
                        maquina[maq_origem].insert(maquina[maq_origem].begin() + i, op);
                        return r0;
                    }

                    maquina[m].insert(maquina[m].begin() + j, op);
                    if (buscasDebugEnabled())
                    {
                        std::cout << "[DEBUG][ISIM] Foi pra avaliação " << std::endl;
                    }
                    double resultadoNovo = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_teste);
                    if (buscasDebugEnabled())
                    {
                        std::cout << "[DEBUG][ISIM] Voltou da avaliação " << std::endl;
                    }
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
                }
            }

            maquina[maq_origem].insert(maquina[maq_origem].begin() + i, op);
        }
    }

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ISIM] Nenhuma melhoria encontrada. Retornando objetivo inicial=" << r0 << std::endl;
    }
    return r0;
}

double two_swap(std::vector<std::vector<Operation>> &maquina,
                std::vector<Operation> &vetOperacoes,
                std::map<int, std::map<int, int>> &controleOp,
                std::vector<double> &tardiness_maq)
{
    double r0 = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_maq);
    double resultadoAtual = r0;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][2SWAP] Objetivo inicial: " << r0 << std::endl;
    }

    int numMaquinas = maquina.size();

    std::vector<int> indicesMaquinas(numMaquinas);
    std::iota(indicesMaquinas.begin(), indicesMaquinas.end(), 0);
    std::shuffle(indicesMaquinas.begin(), indicesMaquinas.end(), rng);
    std::stable_sort(indicesMaquinas.begin(), indicesMaquinas.end(), [&](int a, int b)
                     { return tardiness_maq[a] > tardiness_maq[b]; });

    std::vector<double> tardiness_teste = tardiness_maq;

    for (int m : indicesMaquinas)
    {
        if (tempoLimiteAtingido())
            return r0;

        size_t n = maquina[m].size();
        if (n < 2)
            continue;

        for (size_t i = 0; i < n - 1; i++)
        {
            if (tempoLimiteAtingido())
                return r0;

            for (size_t j = i + 1; j < n; j++)
            {
                if (tempoLimiteAtingido())
                    return r0;

                std::swap(maquina[m][i], maquina[m][j]);

                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][2SWap] Foi avaliar " << std::endl;
                }
                double novoResultado = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_teste);
                if (buscasDebugEnabled())
                {
                    std::cout << "[DEBUG][2SWap] Voltou da avaliação " << std::endl;
                }
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

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][2SWap] Nenhuma melhoria encontrada. Retornando objetivo inicial=" << r0 << std::endl;
    }

    return r0;
}


