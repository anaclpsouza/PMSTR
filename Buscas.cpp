#include "Buscas.h"
#include "ObjectiveFunction.h"
#include "Operation.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

std::chrono::high_resolution_clock::time_point inicioExecucao;
static std::chrono::high_resolution_clock::duration limiteTempoExecucao = std::chrono::hours(2);
static std::mt19937 rng(std::random_device{}());

static inline bool tempoLimiteAtingido()
{
    return (std::chrono::high_resolution_clock::now() - inicioExecucao) >= limiteTempoExecucao;
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

static std::vector<int> ordenarMaquinasPorAtraso(int totalMaquinas,
                                                 const std::vector<double> &atrasoPorMaquina,
                                                 bool decrescente)
{
    std::vector<int> indices(totalMaquinas);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), rng);
    std::stable_sort(indices.begin(), indices.end(), [&](int a, int b)
                     {
                         return decrescente
                                    ? (atrasoPorMaquina[a] > atrasoPorMaquina[b])
                                    : (atrasoPorMaquina[a] < atrasoPorMaquina[b]);
                     });
    return indices;
}

double ILS(int usarReInsertion,
           int usarInsertionIm,
           int usarTwoSwap,
           int limiteIteracoesSemMelhoria,
           double percentualPerturbacao,
           int limiteTempoHoras,
           std::vector<std::vector<Operation>> &solucaoMaquinas,
           const std::vector<Operation> &operacoes,
           std::vector<double> &atrasoPorMaquina,
           int totalOperacoes)
{
    auto executarBusca = [&](int tipoBusca)
    {
        if (tipoBusca == 0)
            return re_insertion(solucaoMaquinas, operacoes, atrasoPorMaquina);
        if (tipoBusca == 1)
            return insertion_im(solucaoMaquinas, operacoes, atrasoPorMaquina);
        return two_swap(solucaoMaquinas, operacoes, atrasoPorMaquina);
    };

    limiteTempoExecucao = std::chrono::hours(limiteTempoHoras);

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Iniciando ILS com " << solucaoMaquinas.size()
                  << " maquinas e " << totalOperacoes << " operacoes" << std::endl;
    }

    double custoBase = objectiveFunction(solucaoMaquinas, operacoes, atrasoPorMaquina);

    if (usarReInsertion == 1 && !tempoLimiteAtingido())
        custoBase = executarBusca(0);
    if (usarInsertionIm == 1 && !tempoLimiteAtingido())
        custoBase = executarBusca(1);
    if (usarTwoSwap == 1 && !tempoLimiteAtingido())
        custoBase = executarBusca(2);

    double melhorCusto = custoBase;
    std::vector<std::vector<Operation>> melhorSolucao = solucaoMaquinas;
    std::vector<double> melhorAtrasoPorMaquina = atrasoPorMaquina;
    std::vector<std::vector<Operation>> solucaoBase = solucaoMaquinas;
    std::vector<double> atrasoBase = atrasoPorMaquina;

    int iteracoesExecutadas = 0;
    int iteracoesSemMelhoria = 0;
    bool parouPorTempo = false;

    const int qtdPerturbacoes = std::max(
        1,
        static_cast<int>(std::ceil(totalOperacoes * percentualPerturbacao)));

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Solucao inicial intensificada=" << custoBase
                  << " | perturbacao=" << qtdPerturbacoes
                  << " | limite sem melhoria=" << limiteIteracoesSemMelhoria
                  << std::endl;
    }

    while (iteracoesSemMelhoria < limiteIteracoesSemMelhoria)
    {
        if (tempoLimiteAtingido())
        {
            parouPorTempo = true;
            break;
        }

        ++iteracoesExecutadas;

        solucaoMaquinas = solucaoBase;
        atrasoPorMaquina = atrasoBase;

        std::vector<std::pair<int, Operation>> operacoesEmEspera;
        std::vector<double> atrasoTemporario;
        objectiveFunction(solucaoMaquinas, operacoes, atrasoTemporario, &operacoesEmEspera);

        double custoAtual = perturbacao_enviesada(solucaoMaquinas,
                                                  operacoes,
                                                  atrasoPorMaquina,
                                                  operacoesEmEspera,
                                                  qtdPerturbacoes);

        int indiceVizinhanca = 0;
        while (indiceVizinhanca < 3)
        {
            if (tempoLimiteAtingido())
            {
                parouPorTempo = true;
                break;
            }

            if ((indiceVizinhanca == 0 && usarReInsertion == 0) ||
                (indiceVizinhanca == 1 && usarInsertionIm == 0) ||
                (indiceVizinhanca == 2 && usarTwoSwap == 0))
            {
                ++indiceVizinhanca;
                continue;
            }

            const double custoCandidato = executarBusca(indiceVizinhanca);
            if (custoCandidato < custoAtual)
            {
                custoAtual = custoCandidato;
                indiceVizinhanca = 0;
            }
            else
            {
                ++indiceVizinhanca;
            }
        }

        if (custoAtual < melhorCusto)
        {
            melhorCusto = custoAtual;
            melhorSolucao = solucaoMaquinas;
            melhorAtrasoPorMaquina = atrasoPorMaquina;
            iteracoesSemMelhoria = 0;
        }
        else
        {
            ++iteracoesSemMelhoria;
        }

        // Critério de aceitação: aceita como nova base se estiver dentro de +10% do melhor global.
        if (custoAtual < INT_MAX && custoAtual <= (melhorCusto * 1.10))
        {
            solucaoBase = solucaoMaquinas;
            atrasoBase = atrasoPorMaquina;
        }

        if (parouPorTempo)
            break;
    }

    solucaoMaquinas = melhorSolucao;
    atrasoPorMaquina = melhorAtrasoPorMaquina;

    if (buscasDebugEnabled())
    {
        std::cout << "[DEBUG][ILS] Encerrando ILS com melhor=" << melhorCusto
                  << " | iteracoes=" << iteracoesExecutadas
                  << " | sem melhoria finais=" << iteracoesSemMelhoria << std::endl;
    }

    return melhorCusto;
}

double perturbacao_enviesada(std::vector<std::vector<Operation>> &solucaoMaquinas,
                             const std::vector<Operation> &operacoes,
                             std::vector<double> &atrasoPorMaquina,
                             const std::vector<std::pair<int, Operation>> &operacoesEmEspera,
                             int qtdPerturbacoes)
{
    const int totalMaqs = static_cast<int>(solucaoMaquinas.size());
    std::vector<std::pair<int, Operation>> selecionadas;

    for (const auto &item : operacoesEmEspera)
    {
        if (static_cast<int>(selecionadas.size()) >= qtdPerturbacoes)
            break;
        selecionadas.push_back(item);
    }

    while (static_cast<int>(selecionadas.size()) < qtdPerturbacoes)
    {
        const int maqAleatoria = std::uniform_int_distribution<>(0, totalMaqs - 1)(rng);
        if (solucaoMaquinas[maqAleatoria].empty())
            continue;
        const int posAleatoria = std::uniform_int_distribution<>(
            0, static_cast<int>(solucaoMaquinas[maqAleatoria].size()) - 1)(rng);
        selecionadas.emplace_back(maqAleatoria, solucaoMaquinas[maqAleatoria][posAleatoria]);
    }

    std::vector<double> atrasoTeste = atrasoPorMaquina;

    for (const auto &item : selecionadas)
    {
        if (tempoLimiteAtingido())
            break;

        const int maqOrigem = item.first;
        const Operation &opAlvo = item.second;

        int posOrigem = -1;
        for (auto it = solucaoMaquinas[maqOrigem].begin(); it != solucaoMaquinas[maqOrigem].end(); ++it)
        {
            if (it->idJob == opAlvo.idJob && it->idOp == opAlvo.idOp)
            {
                posOrigem = static_cast<int>(std::distance(solucaoMaquinas[maqOrigem].begin(), it));
                solucaoMaquinas[maqOrigem].erase(it);
                break;
            }
        }
        if (posOrigem == -1)
            continue;

        int maqDestino = maqOrigem;
        if (totalMaqs > 1)
        {
            while (maqDestino == maqOrigem)
                maqDestino = std::uniform_int_distribution<>(0, totalMaqs - 1)(rng);
        }

        int melhorPosicao = -1;
        double melhorCusto = std::numeric_limits<double>::max();

        std::vector<int> ordemPosicoes(solucaoMaquinas[maqDestino].size() + 1);
        std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
        std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

        for (int pos : ordemPosicoes)
        {
            if (tempoLimiteAtingido())
                break;

            solucaoMaquinas[maqDestino].insert(solucaoMaquinas[maqDestino].begin() + pos, opAlvo);
            const double custoCandidato = objectiveFunction(solucaoMaquinas, operacoes, atrasoTeste);

            if (custoCandidato < melhorCusto && custoCandidato < INT_MAX)
            {
                melhorCusto = custoCandidato;
                melhorPosicao = pos;
            }

            solucaoMaquinas[maqDestino].erase(solucaoMaquinas[maqDestino].begin() + pos);
        }

        if (melhorPosicao != -1)
            solucaoMaquinas[maqDestino].insert(solucaoMaquinas[maqDestino].begin() + melhorPosicao, opAlvo);
        else
            solucaoMaquinas[maqOrigem].insert(solucaoMaquinas[maqOrigem].begin() + posOrigem, opAlvo);
    }

    return objectiveFunction(solucaoMaquinas, operacoes, atrasoPorMaquina);
}

double re_insertion(std::vector<std::vector<Operation>> &solucaoMaquinas,
                    const std::vector<Operation> &operacoes,
                    std::vector<double> &atrasoPorMaquina)
{
    const double custoInicial = objectiveFunction(solucaoMaquinas, operacoes, atrasoPorMaquina);
    const std::vector<int> maquinasOrdenadas = ordenarMaquinasPorAtraso(
        static_cast<int>(solucaoMaquinas.size()), atrasoPorMaquina, true);

    std::vector<double> atrasoTeste = atrasoPorMaquina;

    for (int idxMaq : maquinasOrdenadas)
    {
        if (tempoLimiteAtingido())
            return custoInicial;

        const int numOpsMaq = static_cast<int>(solucaoMaquinas[idxMaq].size());
        if (numOpsMaq < 2)
            continue;

        std::vector<int> ordemPosicoes(numOpsMaq);
        std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
        std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

        for (int posOrigem : ordemPosicoes)
        {
            if (tempoLimiteAtingido())
                return custoInicial;

            for (int posDestino = 0; posDestino < numOpsMaq; ++posDestino)
            {
                if (posOrigem == posDestino)
                    continue;
                if (tempoLimiteAtingido())
                    return custoInicial;

                const Operation opMovida = solucaoMaquinas[idxMaq][posOrigem];
                solucaoMaquinas[idxMaq].erase(solucaoMaquinas[idxMaq].begin() + posOrigem);
                solucaoMaquinas[idxMaq].insert(solucaoMaquinas[idxMaq].begin() + posDestino, opMovida);

                const double custoNovo = objectiveFunction(solucaoMaquinas, operacoes, atrasoTeste);
                if (custoNovo < custoInicial)
                {
                    atrasoPorMaquina = atrasoTeste;
                    return custoNovo;
                }

                solucaoMaquinas[idxMaq].erase(solucaoMaquinas[idxMaq].begin() + posDestino);
                solucaoMaquinas[idxMaq].insert(solucaoMaquinas[idxMaq].begin() + posOrigem, opMovida);
            }
        }
    }

    return custoInicial;
}

double insertion_im(std::vector<std::vector<Operation>> &solucaoMaquinas,
                    const std::vector<Operation> &operacoes,
                    std::vector<double> &atrasoPorMaquina)
{
    const double custoInicial = objectiveFunction(solucaoMaquinas, operacoes, atrasoPorMaquina);
    const std::vector<int> maquinasDestino = ordenarMaquinasPorAtraso(
        static_cast<int>(solucaoMaquinas.size()), atrasoPorMaquina, false);
    const std::vector<int> maquinasOrigem(maquinasDestino.rbegin(), maquinasDestino.rend());

    std::vector<double> atrasoTeste = atrasoPorMaquina;

    for (int maqOrigem : maquinasOrigem)
    {
        if (tempoLimiteAtingido())
            return custoInicial;

        for (size_t posOrigem = 0; posOrigem < solucaoMaquinas[maqOrigem].size(); ++posOrigem)
        {
            if (tempoLimiteAtingido())
                return custoInicial;

            const Operation opMovida = solucaoMaquinas[maqOrigem][posOrigem];
            solucaoMaquinas[maqOrigem].erase(solucaoMaquinas[maqOrigem].begin() + posOrigem);

            for (int maqDestino : maquinasDestino)
            {
                if (maqDestino == maqOrigem)
                    continue;

                if (tempoLimiteAtingido())
                {
                    solucaoMaquinas[maqOrigem].insert(solucaoMaquinas[maqOrigem].begin() + posOrigem, opMovida);
                    return custoInicial;
                }

                std::vector<int> ordemPosicoes(solucaoMaquinas[maqDestino].size() + 1);
                std::iota(ordemPosicoes.begin(), ordemPosicoes.end(), 0);
                std::shuffle(ordemPosicoes.begin(), ordemPosicoes.end(), rng);

                for (int posDestino : ordemPosicoes)
                {
                    if (tempoLimiteAtingido())
                    {
                        solucaoMaquinas[maqOrigem].insert(solucaoMaquinas[maqOrigem].begin() + posOrigem, opMovida);
                        return custoInicial;
                    }

                    solucaoMaquinas[maqDestino].insert(solucaoMaquinas[maqDestino].begin() + posDestino, opMovida);
                    const double custoNovo = objectiveFunction(solucaoMaquinas, operacoes, atrasoTeste);

                    if (custoNovo < custoInicial)
                    {
                        atrasoPorMaquina = atrasoTeste;
                        return custoNovo;
                    }

                    solucaoMaquinas[maqDestino].erase(solucaoMaquinas[maqDestino].begin() + posDestino);
                }
            }

            solucaoMaquinas[maqOrigem].insert(solucaoMaquinas[maqOrigem].begin() + posOrigem, opMovida);
        }
    }

    return custoInicial;
}

double two_swap(std::vector<std::vector<Operation>> &solucaoMaquinas,
                const std::vector<Operation> &operacoes,
                std::vector<double> &atrasoPorMaquina)
{
    const double custoInicial = objectiveFunction(solucaoMaquinas, operacoes, atrasoPorMaquina);
    const std::vector<int> maquinasOrdenadas = ordenarMaquinasPorAtraso(
        static_cast<int>(solucaoMaquinas.size()), atrasoPorMaquina, true);

    std::vector<double> atrasoTeste = atrasoPorMaquina;

    for (int idxMaq : maquinasOrdenadas)
    {
        if (tempoLimiteAtingido())
            return custoInicial;

        const size_t numOpsMaq = solucaoMaquinas[idxMaq].size();
        if (numOpsMaq < 2)
            continue;

        for (size_t i = 0; i < numOpsMaq - 1; ++i)
        {
            if (tempoLimiteAtingido())
                return custoInicial;

            for (size_t j = i + 1; j < numOpsMaq; ++j)
            {
                if (tempoLimiteAtingido())
                    return custoInicial;

                std::swap(solucaoMaquinas[idxMaq][i], solucaoMaquinas[idxMaq][j]);

                const double custoNovo = objectiveFunction(solucaoMaquinas, operacoes, atrasoTeste);
                if (custoNovo < custoInicial)
                {
                    atrasoPorMaquina = atrasoTeste;
                    return custoNovo;
                }

                std::swap(solucaoMaquinas[idxMaq][i], solucaoMaquinas[idxMaq][j]);
            }
        }
    }

    return custoInicial;
}
