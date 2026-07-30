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
#include <deque>
#include <iomanip>

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
           int totalOperacoes,
           std::ostream *logExecucao)
{
    struct EstatisticaBusca
    {
        const char *nome;
        long long chamadas = 0;
        long long resultadosValidos = 0;
        long long retornosInvalidos = 0;
        long long recuperacoesDeSolucaoInvalida = 0;
        long long chamadasComparaveis = 0;
        long long chamadasComMelhoria = 0;
        double somaValoresRetornadosValidos = 0.0;
        double somaMelhorias = 0.0;
        double somaMelhoriasPositivas = 0.0;

        static bool valorValido(double valor)
        {
            return std::isfinite(valor) && valor < static_cast<double>(INT_MAX);
        }

        void registrar(double antes, double depois)
        {
            ++chamadas;

            const bool antesValido = valorValido(antes);
            const bool depoisValido = valorValido(depois);

            if (depoisValido)
            {
                ++resultadosValidos;
                somaValoresRetornadosValidos += depois;
            }
            else
            {
                ++retornosInvalidos;
            }

            if (!antesValido && depoisValido)
                ++recuperacoesDeSolucaoInvalida;

            // INT_MAX nao entra nas medias.
            if (antesValido && depoisValido)
            {
                ++chamadasComparaveis;
                const double melhoria = std::max(0.0, antes - depois);
                somaMelhorias += melhoria;

                if (melhoria > 0.0)
                {
                    ++chamadasComMelhoria;
                    somaMelhoriasPositivas += melhoria;
                }
            }
        }

        double mediaValorRetornado() const
        {
            return resultadosValidos == 0
                       ? 0.0
                       : somaValoresRetornadosValidos / resultadosValidos;
        }

        double mediaMelhoriaPorChamadaComparavel() const
        {
            return chamadasComparaveis == 0
                       ? 0.0
                       : somaMelhorias / chamadasComparaveis;
        }

        double mediaMelhoriaQuandoMelhora() const
        {
            return chamadasComMelhoria == 0
                       ? 0.0
                       : somaMelhoriasPositivas / chamadasComMelhoria;
        }
    };

    struct RegistroBusca
    {
        int iteracao;
        std::string fase;
        std::string busca;
        double valorAntes;
        double valorDepois;
        double melhoriaComparavel;
        bool antesValido;
        bool depoisValido;
        bool recuperouSolucaoInvalida;
    };

    struct RegistroIteracao
    {
        int iteracao;
        double valorBaseInicio;
        double aposPerturbacao;
        double aposBuscas;
        double melhorGlobal;
        bool melhorouGlobal;
        int semMelhoriaConsecutivas;
        bool aceitaComoBase;
    };

    EstatisticaBusca estatReInsertion{"re_insertion"};
    EstatisticaBusca estatInsertionIm{"insertion_im"};
    EstatisticaBusca estatTwoSwap{"two_swap"};
    std::vector<RegistroBusca> registrosBuscas;
    std::vector<RegistroIteracao> registrosIteracoes;

    auto executarBusca = [&](int tipo,
                             double valorAntes,
                             int iteracao,
                             const char *fase)
    {
        double valorDepois = valorAntes;
        EstatisticaBusca *estatistica = nullptr;

        if (tipo == 0)
        {
            valorDepois = re_insertion(maquina, vetOperacoes, controleOp, tardiness_maq);
            estatistica = &estatReInsertion;
        }
        else if (tipo == 1)
        {
            valorDepois = insertion_im(maquina, vetOperacoes, controleOp, tardiness_maq);
            estatistica = &estatInsertionIm;
        }
        else
        {
            valorDepois = two_swap(maquina, vetOperacoes, controleOp, tardiness_maq);
            estatistica = &estatTwoSwap;
        }

        estatistica->registrar(valorAntes, valorDepois);

        const bool antesValido = EstatisticaBusca::valorValido(valorAntes);
        const bool depoisValido = EstatisticaBusca::valorValido(valorDepois);
        const double melhoriaComparavel =
            (antesValido && depoisValido)
                ? std::max(0.0, valorAntes - valorDepois)
                : 0.0;

        registrosBuscas.push_back({iteracao,
                                   fase,
                                   estatistica->nome,
                                   valorAntes,
                                   valorDepois,
                                   melhoriaComparavel,
                                   antesValido,
                                   depoisValido,
                                   !antesValido && depoisValido});
        return valorDepois;
    };

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Iniciando ILS com " << maquina.size()
                  << " maquinas e " << totalOperacoes << " operacoes" << std::endl;
    }

    // Intensificacao inicial. A iteracao 0 identifica esta fase no log.
    const double valorAntesIntensificacao =
        objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_maq);
    double s = valorAntesIntensificacao;

    if (usarReInsertion == 1)
        s = executarBusca(0, s, 0, "intensificacao_inicial");
    if (usarInsertionIm == 1)
        s = executarBusca(1, s, 0, "intensificacao_inicial");
    if (usarTwoSwap == 1)
        s = executarBusca(2, s, 0, "intensificacao_inicial");

    double melhor = s;
    std::vector<std::vector<Operation>> melhorSolucao = maquina;
    std::vector<double> melhorTardiness = tardiness_maq;
    std::vector<std::vector<Operation>> solucaoBase = maquina;
    std::vector<double> tardinessBase = tardiness_maq;

    int iteracaoMelhorSolucao = 0;
    int iteracaoMaiorMelhoriaGlobal = -1;
    double maiorMelhoriaGlobal = 0.0;

    const double melhoriaIntensificacaoInicial =
        std::max(0.0, valorAntesIntensificacao - s);
    if (melhoriaIntensificacaoInicial > 0.0)
    {
        iteracaoMaiorMelhoriaGlobal = 0;
        maiorMelhoriaGlobal = melhoriaIntensificacaoInicial;
    }

    int iteracoesExecutadas = 0;
    int iteracoesSemMelhoriaConsecutivas = 0;
    int totalIteracoesSemMelhoria = 0;
    int maxIteracoesSemMelhoriaConsecutivas = 0;
    bool parouPorTempo = false;

    const int quantidadePerturbacoes = std::max(
        2,
        static_cast<int>(std::ceil(totalOperacoes * percentualPerturbacao)));

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Solucao inicial intensificada=" << s
                  << " | perturbacao=" << quantidadePerturbacoes
                  << " | limite sem melhoria=" << limiteIteracoesSemMelhoria
                  << std::endl;
    }

    // iteracoes CONSECUTIVAS sem melhoria global.
    // Quando uma nova melhor solucao e encontrada, o contador volta para zero.
    while (iteracoesSemMelhoriaConsecutivas < limiteIteracoesSemMelhoria)
    {
        t2 = high_resolution_clock::now();
        tempo_execucao = t2 - t1;

        if (tempo_execucao >= std::chrono::hours(limiteTempoHoras))
        {
            parouPorTempo = true;
            break;
        }

        const int iteracaoAtual = ++iteracoesExecutadas;
        const double valorBaseInicio = s;

        maquina = solucaoBase;
        tardiness_maq = tardinessBase;

        // Perturbacao 
        double sAtual = pertubacao(maquina,
                                   vetOperacoes,
                                   controleOp,
                                   tardiness_maq,
                                   quantidadePerturbacoes);
        const double valorAposPerturbacao = sAtual;
        double sCandidato = sAtual;

        // VND 
        int qual = 0;
        while (qual < 3)
        {
            if ((qual == 0 && usarReInsertion == 0) ||
                (qual == 1 && usarInsertionIm == 0) ||
                (qual == 2 && usarTwoSwap == 0))
            {
                ++qual;
                continue;
            }

            sCandidato = executarBusca(qual, sAtual, iteracaoAtual, "vnd");

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

        bool melhorouGlobal = false;
        if (sAtual < melhor)
        {
            const double melhoriaGlobal = melhor - sAtual;
            melhor = sAtual;
            melhorSolucao = maquina;
            melhorTardiness = tardiness_maq;
            iteracaoMelhorSolucao = iteracaoAtual;
            iteracoesSemMelhoriaConsecutivas = 0;
            melhorouGlobal = true;

            if (melhoriaGlobal > maiorMelhoriaGlobal)
            {
                maiorMelhoriaGlobal = melhoriaGlobal;
                iteracaoMaiorMelhoriaGlobal = iteracaoAtual;
            }
        }
        else
        {
            ++iteracoesSemMelhoriaConsecutivas;
            ++totalIteracoesSemMelhoria;
            maxIteracoesSemMelhoriaConsecutivas = std::max(
                maxIteracoesSemMelhoriaConsecutivas,
                iteracoesSemMelhoriaConsecutivas);
        }

        const bool aceitaComoBase = sCandidato <= (melhor * 1.10);
        if (aceitaComoBase)
        {
            solucaoBase = maquina;
            tardinessBase = tardiness_maq;
            s = sCandidato;
        }

        registrosIteracoes.push_back({iteracaoAtual,
                                      valorBaseInicio,
                                      valorAposPerturbacao,
                                      sAtual,
                                      melhor,
                                      melhorouGlobal,
                                      iteracoesSemMelhoriaConsecutivas,
                                      aceitaComoBase});
    }

    maquina = melhorSolucao;
    tardiness_maq = melhorTardiness;

    if (logExecucao != nullptr)
    {
        *logExecucao << std::fixed << std::setprecision(6);

        *logExecucao << "\n[CHAMADAS_BUSCAS_LOCAIS]\n";
        *logExecucao << "iteracao;fase;busca;valor_antes;valor_depois;melhoria_comparavel;antes_valido;depois_valido;recuperou_solucao_invalida\n";
        for (const auto &registro : registrosBuscas)
        {
            *logExecucao << registro.iteracao << ';'
                         << registro.fase << ';'
                         << registro.busca << ';'
                         << registro.valorAntes << ';'
                         << registro.valorDepois << ';'
                         << registro.melhoriaComparavel << ';'
                         << (registro.antesValido ? 1 : 0) << ';'
                         << (registro.depoisValido ? 1 : 0) << ';'
                         << (registro.recuperouSolucaoInvalida ? 1 : 0) << '\n';
        }

        *logExecucao << "\n[ITERACOES_ILS]\n";
        *logExecucao << "iteracao;valor_base_inicio;apos_perturbacao;apos_buscas;melhor_global;melhorou_global;sem_melhoria_consecutivas;aceita_como_base\n";
        for (const auto &registro : registrosIteracoes)
        {
            *logExecucao << registro.iteracao << ';'
                         << registro.valorBaseInicio << ';'
                         << registro.aposPerturbacao << ';'
                         << registro.aposBuscas << ';'
                         << registro.melhorGlobal << ';'
                         << (registro.melhorouGlobal ? 1 : 0) << ';'
                         << registro.semMelhoriaConsecutivas << ';'
                         << (registro.aceitaComoBase ? 1 : 0) << '\n';
        }

        auto escreverResumoBusca = [&](const EstatisticaBusca &estatistica)
        {
            *logExecucao << estatistica.nome << ';'
                         << estatistica.chamadas << ';'
                         << estatistica.resultadosValidos << ';'
                         << estatistica.retornosInvalidos << ';'
                         << estatistica.recuperacoesDeSolucaoInvalida << ';'
                         << estatistica.chamadasComparaveis << ';'
                         << estatistica.chamadasComMelhoria << ';'
                         << estatistica.mediaValorRetornado() << ';'
                         << estatistica.mediaMelhoriaPorChamadaComparavel() << ';'
                         << estatistica.mediaMelhoriaQuandoMelhora() << '\n';
        };

        *logExecucao << "\n[RESUMO_BUSCAS_LOCAIS]\n";
        *logExecucao << "busca;chamadas;resultados_validos;retornos_invalidos;recuperacoes_de_solucao_invalida;chamadas_comparaveis;chamadas_com_melhoria;media_valor_retornado_valido;media_melhoria_por_chamada_comparavel;media_melhoria_quando_melhora\n";
        escreverResumoBusca(estatReInsertion);
        escreverResumoBusca(estatInsertionIm);
        escreverResumoBusca(estatTwoSwap);

        const auto maiorMelhoriaBusca = std::max_element(
            registrosBuscas.begin(),
            registrosBuscas.end(),
            [](const RegistroBusca &a, const RegistroBusca &b)
            {
                return a.melhoriaComparavel < b.melhoriaComparavel;
            });

        *logExecucao << "\n[RESUMO_ILS]\n";
        *logExecucao << "melhor_valor;" << melhor << '\n';
        *logExecucao << "iteracao_melhor_solucao;" << iteracaoMelhorSolucao << '\n';
        *logExecucao << "iteracao_maior_melhoria_global;"
                     << iteracaoMaiorMelhoriaGlobal << '\n';
        *logExecucao << "maior_melhoria_global;" << maiorMelhoriaGlobal << '\n';

        if (maiorMelhoriaBusca != registrosBuscas.end())
        {
            *logExecucao << "iteracao_maior_melhoria_busca_local;"
                         << maiorMelhoriaBusca->iteracao << '\n';
            *logExecucao << "busca_maior_melhoria_local;"
                         << maiorMelhoriaBusca->busca << '\n';
            *logExecucao << "maior_melhoria_busca_local;"
                         << maiorMelhoriaBusca->melhoriaComparavel << '\n';
        }

        *logExecucao << "iteracoes_executadas;" << iteracoesExecutadas << '\n';
        *logExecucao << "total_iteracoes_sem_melhoria;"
                     << totalIteracoesSemMelhoria << '\n';
        *logExecucao << "iteracoes_sem_melhoria_finais;"
                     << iteracoesSemMelhoriaConsecutivas << '\n';
        *logExecucao << "max_iteracoes_sem_melhoria_consecutivas;"
                     << maxIteracoesSemMelhoriaConsecutivas << '\n';
        *logExecucao << "motivo_parada;"
                     << (parouPorTempo ? "limite_tempo" : "limite_sem_melhoria")
                     << '\n';
    }

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Encerrando ILS com melhor=" << melhor
                  << " | iteracoes=" << iteracoesExecutadas
                  << " | sem melhoria finais="
                  << iteracoesSemMelhoriaConsecutivas << std::endl;
    }

    return melhor;
}

double pertubacao(std::vector<std::vector<Operation>> &maquina,
                  std::vector<Operation> &vetOperacoes,
                  std::map<int, std::map<int, int>> &controleOp,
                  std::vector<double> &tardiness_maq,
                  int o)
{
    int numTrocas = o;
    int numMaquinas = maquina.size();

    for (int k = 0; k < numTrocas; ++k)
    {
        int m1 = std::uniform_int_distribution<>(0, numMaquinas - 1)(rng);
        int m2 = std::uniform_int_distribution<>(0, numMaquinas - 1)(rng);

        if (m1 == m2 || maquina[m1].empty() || maquina[m2].empty())
        {
            k--; // Tenta novamente
            continue;
        }

        // Escolhe posições aleatórias
        int pos1 = std::uniform_int_distribution<>(0, maquina[m1].size() - 1)(rng);
        int pos2 = std::uniform_int_distribution<>(0, maquina[m2].size() - 1)(rng);

        // Troca as operações entre máquinas (Inter-machine Swap)
        std::swap(maquina[m1][pos1], maquina[m2][pos2]);
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

/* long agrupaOp(std::vector<std::vector<Operation>> &maquina,
              std::vector<Operation> &vetOperacoes,
              std::map<int, std::map<int, int>> &controleOp,
              std::vector<double> &tardiness_maq, std::map<int, std::deque<Operation>> tarefas)
{

    long r0 = objectiveFunction(maquina, vetOperacoes, controleOp, tardiness_maq);
    long resultadoAtual = r0;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][AGRUPAOP] Objetivo inicial: " << r0 << std::endl;
    }

    int numMaquinas = maquina.size();

    std::vector<int> indicesMaquinas(numMaquinas);
    std::iota(indicesMaquinas.begin(), indicesMaquinas.end(), 0);
    std::sort(indicesMaquinas.begin(), indicesMaquinas.end(), [&](int a, int b)
              { return tardiness_maq[a] < tardiness_maq[b]; });

    std::vector<double> tardiness_teste = tardiness_maq;

    for (int m : indicesMaquinas)
    {
        size_t n = maquina[m].size();
        if (n < 2)
            continue;

        std::vector<int> sequenciaInicial = {0};

        for (size_t i = 0; i < n; i++)
        {
            int idJob = maquina[m][i].idJob;
            int idOp = maquina[m][i].idOp;

            if (idOp > 1)
            {
                Operation ant = tarefas[idJob][idOp - 2];
                // ... lógica para agrupar ant ...
            }

            if (idOp < tarefas[idJob].size())
            {
                Operation suc = tarefas[idJob][idOp];

                // ... lógica para agrupar suc ...
            }
        }
    }
} */
