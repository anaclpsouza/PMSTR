
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>
#include "Operation.h"
#include "ObjectiveFunction.h"
#include <chrono>
#include <deque>
#include <random>
#include "Buscas.cpp"

using namespace std;
std::ofstream fileSolution;
int m, o, t, c;
std::vector<std::vector<Operation>> maquinas;
std::map<int, double> tempoMaq;
using namespace std::chrono;

void escreverMatrizFinalCompilada(const std::string &caminhoArquivo, const std::string &nomeInstancia, const std::string &execucao)
{
    std::ofstream out(caminhoArquivo, std::ios::app);
    if (!out.is_open())
    {
        return;
    }

    out << "Execucao: " << execucao << endl;
    out << "Instancia: " << nomeInstancia << endl;
    for (size_t maq = 0; maq < maquinas.size(); ++maq)
    {
        out << "M" << (maq + 1) << ":";
        for (const auto &op : maquinas[maq])
        {
            out << " J" << op.idJob << "-O" << op.idOp;
        }
        out << endl;
    }
    out << endl;
}

 /* faz um aleatorio por tarefa. Ordena as Tarefas (a tarefa leva suas operações junto) aleatoriamente, e distribui um para cada máquina. Se uma tarefa foi, todas as operacoes foi. Depois que cada máquina recebeu uma, a próxima tarefa vai para a máquina com o menor completion time calculado como soma do tempo de processamento da tarefa (que é, somatorio do release time de cada operacao + tempo de processamento). Repete até que todas as tarefas tenham sido alocadas. */

void distInicial(std::map<int, std::deque<Operation>> tarefas)
{
    std::vector<int> tarefasDisponiveis;

    for (auto const &[id, fila] : tarefas)
    {
        tarefasDisponiveis.push_back(id);
    }

    if (tarefasDisponiveis.empty() || m <= 0)
    {
        return;
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(tarefasDisponiveis.begin(), tarefasDisponiveis.end(), g);

    auto cargaTarefa = [&](int idJob)
    {
        double soma = 0.0;
        const auto &ops = tarefas[idJob];

        for (const auto &op : ops)
        {
            soma += op.releaseTime + op.processingTime;
        }

        return soma;
    };

    std::vector<double> completionMaq(m, 0.0);
    size_t idxTarefa = 0;

    size_t tarefasIniciais = std::min((size_t)m, tarefasDisponiveis.size());
    for (size_t maq = 0; maq < tarefasIniciais; ++maq)
    {
        int idJob = tarefasDisponiveis[idxTarefa++];
        auto &ops = tarefas[idJob];

        maquinas[maq].insert(maquinas[maq].end(), ops.begin(), ops.end());
        completionMaq[maq] += cargaTarefa(idJob);
    }

    while (idxTarefa < tarefasDisponiveis.size())
    {
        int melhorMaq = 0;
        for (int maq = 1; maq < m; ++maq)
        {
            if (completionMaq[maq] < completionMaq[melhorMaq])
            {
                melhorMaq = maq;
            }
        }

        int idJob = tarefasDisponiveis[idxTarefa++];
        auto &ops = tarefas[idJob];

        maquinas[melhorMaq].insert(maquinas[melhorMaq].end(), ops.begin(), ops.end());
        completionMaq[melhorMaq] += cargaTarefa(idJob);
    }
}

int parseHeaderValue(string line)
{
    replace(line.begin(), line.end(), ',', ' ');
    stringstream ss(line);
    ss.imbue(locale("C"));
    string key;
    int value;
    ss >> key >> value;
    return value;
}

int main(int argsc, char *argv[])
{
    if (argsc < 2)
    {
        std::cerr << "Arquivo de saida nao informado." << std::endl;
        return 1;
    }

    ios_base::sync_with_stdio(false);
    cin.imbue(locale("C"));

    string line;

    if (getline(cin, line))
        o = parseHeaderValue(line);
    if (getline(cin, line))
        m = parseHeaderValue(line);
    if (getline(cin, line))
        t = parseHeaderValue(line);
    if (getline(cin, line))
        c = parseHeaderValue(line);

    getline(cin, line);
    getline(cin, line);

    std::vector<Operation> vetOperacao;
    int i = 0;
    std::map<int, std::map<int, int>> controleOp;

    fileSolution.open(argv[1]);
    controleOp.clear();

    while (getline(cin, line) && !line.empty())
    {
        replace(line.begin(), line.end(), ',', ' ');
        stringstream ss(line);
        ss.imbue(locale("C"));

        int idJob, idOp, toolSetId, toolSetSize;
        double releaseTime, processingTime, dueDate;

        ss >> idJob >> idOp >> releaseTime >> processingTime >> dueDate >> toolSetId >> toolSetSize;

        if (ss)
        {
            Operation op(i, idJob, idOp, toolSetId - 1, toolSetSize, processingTime, dueDate, releaseTime);
            vetOperacao.push_back(op);
            controleOp[idJob][idOp] = 0;
            i++;
        }
    }

    std::map<int, double> tempo_final;
    std::vector<double> tardiness_maq;
    std::map<int, std::deque<Operation>> tarefas;
    for (const auto &op : vetOperacao)
    {
        tarefas[op.idJob].push_back(op);
    }

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

   
    double sol_inicial = INT_MAX;
    while (sol_inicial == INT_MAX)
    {
        maquinas.clear();
        maquinas.resize(m);
        distInicial(tarefas);
        sol_inicial = objectiveFunction(maquinas, vetOperacao, controleOp, tardiness_maq);
    }
    double ils = ILS(maquinas, vetOperacao, controleOp, tardiness_maq, o);

    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    auto tempo_execucao = duration_cast<duration<double>>(t2 - t1);

    fileSolution
        << "Instance_name,O,M,T,C,Solucao_Inicial,ILS,Tempo de_execucao(s)" << endl
        << m << "M" << o << ","
        << o << ","
        << m << ","
        << t << ","
        << c << ","
        << sol_inicial << ","
        << ils << ","
        << tempo_execucao.count() << endl;

    fileSolution.close();

    if (argsc >= 5)
    {
        escreverMatrizFinalCompilada(argv[2], argv[3], argv[4]);
    }

    return 0;
}