#include "ObjectiveFunction.h"
#include "Operation.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <utility>
#include <vector>

EstatisticasKTNS estatisticasKtns;

void resetarEstatisticasKtns()
{
    estatisticasKtns = {0, 0, 0};
}

static inline bool objectiveDebugEnabled()
{
    const char *env = std::getenv("PMSTR_DEBUG_LOCAL_SEARCH");
    return env != nullptr && env[0] != '\0' && env[0] != '0';
}

static int encontrarMaquinaPredecessora(int idJob,
                                       int idOpPredecessora,
                                       const std::vector<std::vector<Operation>> &solucaoMaquinas)
{
    for (int maq = 0; maq < static_cast<int>(solucaoMaquinas.size()); ++maq)
    {
        auto it = std::find_if(solucaoMaquinas[maq].begin(),
                               solucaoMaquinas[maq].end(),
                               [idJob, idOpPredecessora](const Operation &op)
                               {
                                   return op.idJob == idJob && op.idOp == idOpPredecessora;
                               });
        if (it != solucaoMaquinas[maq].end())
            return maq;
    }
    return -1;
}

static bool possuiCicloDependencia(const std::vector<int> &esperandoMaquina)
{
    // 0 = não visitado, 1 = na pilha de recursão, 2 = concluído
    std::vector<int> estado(esperandoMaquina.size(), 0);

    for (size_t inicio = 0; inicio < esperandoMaquina.size(); ++inicio)
    {
        if (estado[inicio] != 0)
            continue;

        int atual = static_cast<int>(inicio);
        while (atual != -1 && estado[atual] != 2)
        {
            if (estado[atual] == 1)
                return true;
            estado[atual] = 1;
            atual = esperandoMaquina[atual];
        }

        atual = static_cast<int>(inicio);
        while (atual != -1 && estado[atual] == 1)
        {
            estado[atual] = 2;
            atual = esperandoMaquina[atual];
        }
    }

    return false;
}

static int selecionarFerramentaParaRemover(bool usarKtnsNovo,
                                           int estagioAtual,
                                           const std::vector<int> &carregadas,
                                           const std::vector<std::vector<int>> &usoNoEstagio,
                                           const std::vector<std::vector<int>> &prioridadesMaquina,
                                           const std::vector<int> &tamanhoConjuntos)
{
    int ferramentaEscolhida = -1;

    if (usarKtnsNovo)
    {
        // Adaptação do KTNS considerando o tamanho das ferramentas:
        // como a remoção tem custo 0 e cada reinserção futura custa 1 setup,
        // remover ferramentas maiores evita expulsar múltiplas ferramentas menores.
        int maiorTamanhoSemUsoFuturo = -1;
        int maiorTamanhoComUsoFuturo = -1;
        int maiorDistanciaDesempate = -1;

        for (int tool = 0; tool < numConjuntosFerramentas; ++tool)
        {
            if (usoNoEstagio[tool][estagioAtual] == 1 || carregadas[tool] == 0)
                continue;

            const int distancia = prioridadesMaquina[tool][estagioAtual];
            const int tamanho = tamanhoConjuntos[tool];

            if (distancia == -1)
            {
                if (tamanho > maiorTamanhoSemUsoFuturo)
                {
                    maiorTamanhoSemUsoFuturo = tamanho;
                    ferramentaEscolhida = tool;
                }
            }
            else if (maiorTamanhoSemUsoFuturo == -1)
            {
                if (tamanho > maiorTamanhoComUsoFuturo ||
                    (tamanho == maiorTamanhoComUsoFuturo && distancia > maiorDistanciaDesempate))
                {
                    maiorTamanhoComUsoFuturo = tamanho;
                    maiorDistanciaDesempate = distancia;
                    ferramentaEscolhida = tool;
                }
            }
        }
        return ferramentaEscolhida;
    }

    // KTNS original: prioriza ferramentas sem uso futuro (-1) ou com maior distância até o próximo uso.
    int maiorDistancia = 0;
    for (int tool = 0; tool < numConjuntosFerramentas; ++tool)
    {
        if (usoNoEstagio[tool][estagioAtual] == 1 || carregadas[tool] == 0)
            continue;

        const int distancia = prioridadesMaquina[tool][estagioAtual];
        if (distancia == -1)
            return tool;

        if (distancia > maiorDistancia)
        {
            maiorDistancia = distancia;
            ferramentaEscolhida = tool;
        }
    }

    return ferramentaEscolhida;
}

double objectiveFunction(const std::vector<std::vector<Operation>> &solucaoMaquinas,
                         const std::vector<Operation> &operacoes,
                         std::vector<double> &atrasoPorMaquina,
                         std::vector<std::pair<int, Operation>> *operacoesEmEspera)
{
    if (solucaoMaquinas.empty())
    {
        if (objectiveDebugEnabled())
            std::cout << "[DEBUG] Lista de maquinas vazia." << std::endl;
        return 0.0;
    }

    if (static_cast<int>(operacoes.size()) != numOperacoes)
        return INT_MAX;

    // Valida a precedência entre operações do mesmo job alocadas na mesma máquina.
    for (int maq = 0; maq < numMaquinas; ++maq)
    {
        std::map<int, int> ultimaOpPorJob;
        for (const auto &op : solucaoMaquinas[maq])
        {
            if (ultimaOpPorJob.count(op.idJob) && ultimaOpPorJob[op.idJob] > op.idOp)
                return INT_MAX;
            ultimaOpPorJob[op.idJob] = op.idOp;
        }
    }

    constexpr double pesoAtraso = 1.0;
    constexpr double pesoTroca = 1.0;
    constexpr double duracaoTroca = 1.0;

    atrasoPorMaquina.assign(numMaquinas, 0.0);

    int maxJobId = 0;
    int maxOpId = 0;
    for (const auto &op : operacoes)
    {
        maxJobId = std::max(maxJobId, op.idJob);
        maxOpId = std::max(maxOpId, op.idOp);
    }

    std::vector<int> tamanhoConjuntos(numConjuntosFerramentas, 0);
    std::vector<std::vector<std::vector<int>>> magazine(numMaquinas);
    std::vector<std::vector<std::vector<int>>> prioridades(numMaquinas);
    std::vector<int> espacoOcupadoInicial(numMaquinas, 0);
    std::vector<std::vector<int>> carregadosInicial(numMaquinas, std::vector<int>(numConjuntosFerramentas, 0));

    for (int maq = 0; maq < numMaquinas; ++maq)
    {
        const int numOpsMaq = static_cast<int>(solucaoMaquinas[maq].size());
        magazine[maq].assign(numConjuntosFerramentas, std::vector<int>(numOpsMaq, 0));
        prioridades[maq].assign(numConjuntosFerramentas, std::vector<int>(numOpsMaq, 0));

        for (int estagio = 0; estagio < numOpsMaq; ++estagio)
        {
            const Operation &op = solucaoMaquinas[maq][estagio];
            tamanhoConjuntos[op.toolSetId] = op.toolSetSize;
            magazine[maq][op.toolSetId][estagio] = 1;

            if (carregadosInicial[maq][op.toolSetId] == 0 &&
                espacoOcupadoInicial[maq] + op.toolSetSize <= capacidadeMagazine)
            {
                espacoOcupadoInicial[maq] += op.toolSetSize;
                carregadosInicial[maq][op.toolSetId] = 1;
            }
        }

        for (int tool = 0; tool < numConjuntosFerramentas; ++tool)
        {
            for (int estagio = 0; estagio < numOpsMaq; ++estagio)
            {
                if (magazine[maq][tool][estagio] == 1)
                {
                    prioridades[maq][tool][estagio] = 0;
                    continue;
                }

                int distancia = 0;
                bool temUsoFuturo = false;
                for (int futuro = estagio + 1; futuro < numOpsMaq; ++futuro)
                {
                    ++distancia;
                    if (magazine[maq][tool][futuro] == 1)
                    {
                        temUsoFuturo = true;
                        break;
                    }
                }
                prioridades[maq][tool][estagio] = temUsoFuturo ? distancia : -1;
            }
        }
    }

    auto simularKtns = [&](bool usarKtnsNovo,
                           std::vector<double> &atrasoOut,
                           std::vector<std::pair<int, Operation>> *esperaOut) -> double
    {
        int totalTrocas = 0;
        double atrasoTotal = 0.0;
        std::vector<double> tempoMaquina(numMaquinas, 0.0);
        atrasoOut.assign(numMaquinas, 0.0);
        if (esperaOut != nullptr)
            esperaOut->clear();

        std::vector<int> espacoOcupado = espacoOcupadoInicial;
        std::vector<std::vector<int>> carregados = carregadosInicial;
        std::vector<std::vector<double>> tempoConclusao(
            maxJobId + 1,
            std::vector<double>(maxOpId + 1, std::numeric_limits<double>::max()));

        std::vector<int> indiceOpPorMaquina(solucaoMaquinas.size(), 0);
        std::vector<int> esperandoMaquina(solucaoMaquinas.size(), -1);

        while (true)
        {
            bool todasConcluidas = true;
            for (size_t maq = 0; maq < solucaoMaquinas.size(); ++maq)
            {
                if (indiceOpPorMaquina[maq] < static_cast<int>(solucaoMaquinas[maq].size()))
                {
                    todasConcluidas = false;
                    break;
                }
            }
            if (todasConcluidas)
                break;

            bool algumaProcessou = false;

            for (int maq = 0; maq < static_cast<int>(solucaoMaquinas.size()); ++maq)
            {
                const int estagio = indiceOpPorMaquina[maq];
                if (estagio >= static_cast<int>(solucaoMaquinas[maq].size()))
                    continue;

                const Operation &op = solucaoMaquinas[maq][estagio];
                double tempoPredecessora = 0.0;

                if (op.idOp > 1)
                {
                    tempoPredecessora = tempoConclusao[op.idJob][op.idOp - 1];

                    if (tempoPredecessora >= std::numeric_limits<double>::max())
                    {
                        esperandoMaquina[maq] = encontrarMaquinaPredecessora(op.idJob, op.idOp - 1, solucaoMaquinas);
                        if (esperandoMaquina[maq] == -1 || esperandoMaquina[maq] == maq)
                            return INT_MAX;

                        if (esperaOut != nullptr)
                            esperaOut->emplace_back(maq, op);

                        if (possuiCicloDependencia(esperandoMaquina))
                        {
                            if (objectiveDebugEnabled())
                                std::cout << "[DEBUG] Ciclo de precedencia detectado." << std::endl;
                            return INT_MAX;
                        }
                        continue;
                    }
                }

                esperandoMaquina[maq] = -1;
                ++indiceOpPorMaquina[maq];
                algumaProcessou = true;

                tempoMaquina[maq] = std::max({tempoMaquina[maq], tempoPredecessora, op.releaseTime});

                if (carregados[maq][op.toolSetId] == 0)
                {
                    espacoOcupado[maq] += op.toolSetSize;
                    carregados[maq][op.toolSetId] = 1;

                    while (espacoOcupado[maq] > capacidadeMagazine)
                    {
                        const int ferramentaRemover = selecionarFerramentaParaRemover(
                            usarKtnsNovo,
                            estagio,
                            carregados[maq],
                            magazine[maq],
                            prioridades[maq],
                            tamanhoConjuntos);

                        if (ferramentaRemover == -1)
                            return INT_MAX;

                        carregados[maq][ferramentaRemover] = 0;
                        espacoOcupado[maq] -= tamanhoConjuntos[ferramentaRemover];
                    }

                    ++totalTrocas;
                    tempoMaquina[maq] += duracaoTroca;
                }

                tempoMaquina[maq] += op.processingTime;
                tempoConclusao[op.idJob][op.idOp] = tempoMaquina[maq];

                if (tempoMaquina[maq] > op.dueDate)
                {
                    const double atraso = tempoMaquina[maq] - op.dueDate;
                    atrasoOut[maq] += atraso;
                    atrasoTotal += atraso;
                }
            }

            if (!algumaProcessou)
                return INT_MAX;
        }

        if (objectiveDebugEnabled())
        {
            std::cout << "[DEBUG] Tardiness=" << atrasoTotal
                      << " | Setups=" << totalTrocas << std::endl;
        }
        return (pesoAtraso * atrasoTotal) + (pesoTroca * duracaoTroca * totalTrocas);
    };

    std::vector<double> atrasoAntigo;
    std::vector<std::pair<int, Operation>> esperaAntigo;

    const double resultadoAntigo = simularKtns(
        false,
        atrasoAntigo,
        operacoesEmEspera != nullptr ? &esperaAntigo : nullptr);

    if (resultadoAntigo == INT_MAX)
        return INT_MAX;

    std::vector<double> atrasoNovo;
    std::vector<std::pair<int, Operation>> esperaNovo;
    const double resultadoNovo = simularKtns(
        true,
        atrasoNovo,
        operacoesEmEspera != nullptr ? &esperaNovo : nullptr);

    auto aplicarVencedor = [&](long long &contadorVitorias,
                               std::vector<double> &atrasoVencedor,
                               std::vector<std::pair<int, Operation>> &esperaVencedora,
                               double valorVencedor)
    {
        ++contadorVitorias;
        atrasoPorMaquina = std::move(atrasoVencedor);
        if (operacoesEmEspera != nullptr)
            *operacoesEmEspera = std::move(esperaVencedora);
        return valorVencedor;
    };

    if (resultadoNovo == INT_MAX || resultadoAntigo < resultadoNovo)
        return aplicarVencedor(estatisticasKtns.ktnsAntigo, atrasoAntigo, esperaAntigo, resultadoAntigo);

    if (resultadoNovo < resultadoAntigo)
        return aplicarVencedor(estatisticasKtns.ktnsNovo, atrasoNovo, esperaNovo, resultadoNovo);

    return aplicarVencedor(estatisticasKtns.empates, atrasoNovo, esperaNovo, resultadoNovo);
}
