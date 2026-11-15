#include "ObjectiveFunction.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <cstdlib>
#include "Operation.h"
#include <climits>
#include <limits>

using namespace std;

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

int maquinaPai(int idJob, int idOpPai, const std::vector<std::vector<Operation>> &maquina) {
   for (int i = 0; i < maquina.size(); ++i) {
        auto it = std::find_if(maquina[i].begin(), maquina[i].end(), [idJob, idOpPai](const Operation& op) {
            return op.idJob == idJob && op.idOp == idOpPai;
        });

        if (it != maquina[i].end()) {
            return i;
        }
   }
   return -1;
}

double objectiveFunction(const std::vector<std::vector<Operation>> &maquina,
                         const std::vector<Operation> &vetOperacoes,
                         const std::map<int, std::map<int, int>> &controleOp,
                         std::vector<double> &tardiness_maq,
                         std::vector<std::string> *details,
                         std::vector<std::pair<int, Operation>> *out_espera)
{
    if (maquina.empty())
    {
        if (objectiveDebugEnabled())
            std::cout << "[DEBUG] Lista de maquinas vazia." << std::endl;
        return 0.0;
    }

    if (objectiveDebugEnabled())
    {
        cout << "[DEBUG] Validando na função objetivo" << endl;
    }
    

    if (vetOperacoes.size() != o)
    {
        return INT_MAX;
    }

    // conferindo a regra de precedência na mesma máquina de uma vez
    for (int i = 0; i < m; ++i)
    {
        std::map<int, int> ultimaOpDoJob;
        for (const auto &op : maquina[i])
        {
            if (ultimaOpDoJob.count(op.idJob) && ultimaOpDoJob[op.idJob] > op.idOp)
            {
                return INT_MAX;
            }
            ultimaOpDoJob[op.idJob] = op.idOp;
        }
    }

    double wd = 1.0;  // Peso do atraso
    double ws = 1.0;  // Peso da troca
    double tau = 1.0; // Tempo de troca (1 hora)

    tardiness_maq.assign(m, 0.0);

    vector<int> u_inicial(m, 0); // quantidade de ferramentas inicialmente carregadas
    vector<vector<int>> carregados_inicial(m, vector<int>(t, 0));
    vector<vector<vector<int>>> magazine(m, vector<vector<int>>(t, vector<int>())); // magazine de presença de ferramentas
    vector<vector<vector<int>>> prioridades(m, vector<vector<int>>(t, vector<int>()));

    vector<int> setsSize(t);
    int maxJobId = 0;
    int maxOpId = 0;

    for (const auto &op : vetOperacoes)
    {
        if (op.idJob > maxJobId)
            maxJobId = op.idJob;
        if (op.idOp > maxOpId)
            maxOpId = op.idOp;
    }

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
            if (carregados_inicial[j][maquina[j][i].toolSetId] == 0 && u_inicial[j] + maquina[j][i].toolSetSize <= c)
            {
                u_inicial[j] += maquina[j][i].toolSetSize;
                carregados_inicial[j][maquina[j][i].toolSetId] = 1;
            }
        }
    }

    auto simularKtns = [&](bool usarKtnsNovo,
                           std::vector<double> &tardiness_out,
                           std::vector<std::pair<int, Operation>> *espera_out) -> double
    {
        int setups = 0;
        double tardiness = 0.0;
        vector<double> tempo(m, 0.0);
        tardiness_out.assign(m, 0.0);
        if (espera_out != nullptr)
        {
            espera_out->clear();
        }

        vector<int> u = u_inicial;
        vector<vector<int>> carregados = carregados_inicial;
        std::vector<std::vector<double>> tempo_final(
            maxJobId + 1,
            std::vector<double>(maxOpId + 1, std::numeric_limits<double>::max()));

        std::vector<int> idxPorMaquina(maquina.size(), 0);
        vector<int> esperandoPorQuem(maquina.size(), -1);

        bool acabei = false;
        while (!acabei)
        {
            acabei = true;
            for (int i = 0; i < maquina.size(); ++i)
            {
                if (idxPorMaquina[i] < maquina[i].size())
                {
                    if (objectiveDebugEnabled())
                    {
                        cout << "[DEBUG] Operações processadas na máquina " << i << " " << idxPorMaquina[i] << endl;
                        cout << "[DEBUG] Operações totais: " << maquina[i].size() << endl;
                    }

                    acabei = false;
                    break;
                }
            }
            if (acabei)
            {
                if (objectiveDebugEnabled())
                    cout << "[DEBUG] terminei as operações de todas as máquina " << endl;
                break;
            }

            bool algumaProcessou = false;

            for (int i = 0; i < maquina.size(); i++) // máquinas
            {
                int j = idxPorMaquina[i];

                if (j >= maquina[i].size())
                    continue;

                int idJob = maquina[i][j].idJob;
                int idOp = maquina[i][j].idOp;
                bool troca = false;

                double tempoPai = 0.0;
                if (idOp > 1)
                {
                    tempoPai = tempo_final[idJob][idOp - 1];

                    if (tempoPai >= std::numeric_limits<double>::max())
                    {
                        if (objectiveDebugEnabled())
                        {
                            cout << "[DEBUG] Máquina em espera: " << i << " com a operação " << idxPorMaquina[i] << endl;
                        }
                        esperandoPorQuem[i] = maquinaPai(idJob, idOp - 1, maquina);
                        if (esperandoPorQuem[i] == -1 || esperandoPorQuem[i] == i)
                        {
                            return INT_MAX;
                        }
                        vector<int> estado(esperandoPorQuem.size(), 0);

                        if (espera_out != nullptr)
                        {
                            espera_out->push_back(std::make_pair(i, maquina[i][j]));
                        }

                        for (int z = 0; z < esperandoPorQuem.size(); z++)
                        {
                            if (estado[z] == 0)
                            {
                                int atual = z;
                                while (atual != -1 && estado[atual] != 2)
                                {
                                    if (estado[atual] == 1)
                                    {
                                        if (objectiveDebugEnabled())
                                            cout << "[DEBUG] ACHEI UM CICLO E INVALIDEI A SOLUCAO" << endl;
                                        return INT_MAX;
                                    }
                                    estado[atual] = 1;
                                    atual = esperandoPorQuem[atual];
                                }
                                atual = z;
                                while (atual != -1 && estado[atual] == 1)
                                {
                                    estado[atual] = 2;
                                    atual = esperandoPorQuem[atual];
                                }
                            }
                        }

                        continue;
                    }
                }

                if (objectiveDebugEnabled())
                {
                    cout << "[DEBUG] Máquina em processamento: " << i << " com a operação " << idxPorMaquina[i] << " de " << maquina[i].size() << " totais" << endl;
                }

                esperandoPorQuem[i] = -1;
                idxPorMaquina[i]++;
                algumaProcessou = true;

                // Cálculo do tempo considerando a espera
                tempo[i] = std::max({tempo[i], tempoPai, (double)maquina[i][j].releaseTime});

                if (carregados[i][maquina[i][j].toolSetId] == 0)
                {
                    u[i] += maquina[i][j].toolSetSize;
                    carregados[i][maquina[i][j].toolSetId] = 1; // carrega a ferramenta necessária para a operação atual
                    troca = true;

                    while (u[i] > c)
                    {
                        int pEscolhida = -1;

                        if (usarKtnsNovo)
                        {
                            // KTNS Novo (orientado ao tamanho das ferramentas):
                            // Etapa 1: Prioriza remover ferramentas sem uso futuro (prioridade == -1),
                            // desempatando pelo maior tamanho (setsSize[k]).
                            int maiorTamanhoSemUsoFuturo = -1;

                            // Etapa 2: Caso todas tenham uso futuro (prioridade > 0),
                            // escolhe a de maior tamanho, desempatando pela maior distância de reuso.
                            int maiorTamanhoComUsoFuturo = -1;
                            int maiorPrioridadeDesempate = -1;

                            for (unsigned k = 0; k < t; ++k)
                            {
                                if (magazine[i][k][j] != 1 && carregados[i][k] == 1)
                                {
                                    int prio = prioridades[i][k][j];
                                    int tam = setsSize[k];

                                    if (prio == -1)
                                    {
                                        if (tam > maiorTamanhoSemUsoFuturo)
                                        {
                                            maiorTamanhoSemUsoFuturo = tam;
                                            pEscolhida = k;
                                        }
                                    }
                                    else if (maiorTamanhoSemUsoFuturo == -1)
                                    {
                                        if (tam > maiorTamanhoComUsoFuturo ||
                                            (tam == maiorTamanhoComUsoFuturo && prio > maiorPrioridadeDesempate))
                                        {
                                            maiorTamanhoComUsoFuturo = tam;
                                            maiorPrioridadeDesempate = prio;
                                            pEscolhida = k;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            // KTNS Antigo (original: remoção pela maior distância de reuso):
                            int maior = 0;
                            for (unsigned k = 0; k < t; ++k)
                            {
                                if (magazine[i][k][j] != 1)
                                {
                                    if ((carregados[i][k] == 1) && (prioridades[i][k][j] == -1))
                                    {
                                        pEscolhida = k;
                                        break;
                                    }
                                    else
                                    {
                                        if ((prioridades[i][k][j] > maior) && carregados[i][k] == 1)
                                        {
                                            maior = prioridades[i][k][j];
                                            pEscolhida = k;
                                        }
                                    }
                                }
                            }
                        }

                        if (pEscolhida == -1)
                            return INT_MAX;

                        carregados[i][pEscolhida] = 0;
                        u[i] -= setsSize[pEscolhida];
                    }
                }
                if (troca)
                {
                    ++setups;
                    tempo[i] += tau;
                }

                tempo[i] += maquina[i][j].processingTime; // processa a operação (adiciona o processing time)

                tempo_final[idJob][idOp] = tempo[i];

                if (tempo[i] > maquina[i][j].dueDate)
                {
                    double atraso = tempo[i] - maquina[i][j].dueDate;
                    tardiness_out[i] += atraso; // Acumula na máquina específica
                    tardiness += atraso;
                }
            }

            if (!algumaProcessou)
            {
                return INT_MAX;
            }
        }

        if (objectiveDebugEnabled())
            std::cout << "[DEBUG] Tardiness final: " << tardiness << " | setups=" << setups << std::endl;
        return (wd * tardiness) + (ws * tau * setups);
    };

    std::vector<double> tardinessAntigo;
    std::vector<std::pair<int, Operation>> esperaAntigo;
    double resultadoAntigo = simularKtns(false, tardinessAntigo, out_espera != nullptr ? &esperaAntigo : nullptr);

    if (resultadoAntigo == INT_MAX)
    {
        return INT_MAX;
    }

    std::vector<double> tardinessNovo;
    std::vector<std::pair<int, Operation>> esperaNovo;
    double resultadoNovo = simularKtns(true, tardinessNovo, out_espera != nullptr ? &esperaNovo : nullptr);

    if (resultadoNovo == INT_MAX)
    {
        ++estatisticasKtns.ktnsAntigo;
        tardiness_maq = std::move(tardinessAntigo);
        if (out_espera != nullptr)
            *out_espera = std::move(esperaAntigo);
        return resultadoAntigo;
    }

    if (resultadoAntigo < resultadoNovo)
    {
        ++estatisticasKtns.ktnsAntigo;
        tardiness_maq = std::move(tardinessAntigo);
        if (out_espera != nullptr)
            *out_espera = std::move(esperaAntigo);
        return resultadoAntigo;
    }
    else if (resultadoNovo < resultadoAntigo)
    {
        ++estatisticasKtns.ktnsNovo;
        tardiness_maq = std::move(tardinessNovo);
        if (out_espera != nullptr)
            *out_espera = std::move(esperaNovo);
        return resultadoNovo;
    }
    else
    {
        ++estatisticasKtns.empates;
        tardiness_maq = std::move(tardinessNovo);
        if (out_espera != nullptr)
            *out_espera = std::move(esperaNovo);
        return resultadoNovo;
    }
}