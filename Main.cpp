
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

using namespace std;
std::ofstream fileSolution;
int m, o, t, c;
std::vector<std::vector<Operation>> maquinas;
std::map<int, double> tempoMaq;
using namespace std::chrono;

std::vector<Operation> randomizarOp(std::vector<Operation> vetOperacao)
{
    std::map<int, std::deque<Operation>> tarefas;
    for (const auto &op : vetOperacao)
    {
        tarefas[op.idOp].push_back(op);
    }

    std::vector<Operation> operacoesRandomizadas;
    std::vector<int> tarefasDisponiveis;

    for (auto const &[id, fila] : tarefas)
    {
        tarefasDisponiveis.push_back(id);
    }

    std::random_device rd;
    std::mt19937 g(rd());

    while (!tarefasDisponiveis.empty())
    {

        std::uniform_int_distribution<> dis(0, tarefasDisponiveis.size() - 1);
        int indexSorteado = dis(g);
        int taskIdSorteada = tarefasDisponiveis[indexSorteado];

        auto &fila = tarefas[taskIdSorteada];
        operacoesRandomizadas.push_back(fila.front());
        fila.pop_front();

        if (fila.empty())
        {
            tarefasDisponiveis.erase(tarefasDisponiveis.begin() + indexSorteado);
        }
    }

    return operacoesRandomizadas;
}

void atribuirMaquinas(std::vector<Operation> operacoes)
{
    for (const auto &op : operacoes)
    {
        int melhorMaq = 0;
        double menorTempo = -1.0;

        for (int j = 0; j < m; j++)
        {

            double tempoCalculado = tempoMaq[j] + op.releaseTime + op.processingTime;

            if (menorTempo < 0 || tempoCalculado < menorTempo)
            {
                menorTempo = tempoCalculado;
                melhorMaq = j;
            }
        }

        maquinas[melhorMaq].push_back(op);
        tempoMaq[melhorMaq] += (op.releaseTime + op.processingTime);
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
    int operacao_global = 0;
    std::map<int, std::map<int, int>> controleOp;
    maquinas.resize(m);

    fileSolution.open(argv[1]);

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
            operacao_global++;
        }

        std::vector<Operation> opsAleatorias = randomizarOp(vetOperacao);
        atribuirMaquinas(opsAleatorias);
    }

    std::map<int, double> tempo_cinal;
    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    double objective = objectiveFunction(maquinas, tempo_cinal, vetOperacao, controleOp);

    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    auto tempo_execucao = duration_cast<duration<double>>(t2 - t1);

    fileSolution
        << "Instance_name,O,M,T,C,Solucao_Inicial,Tempo de_execucao(s)" << endl
        << m << "M" << o << ","
        << o << ","
        << m << ","
        << t << ","
        << c << ","
        << objective << ","
        << tempo_execucao.count() << endl;

    fileSolution.close();

    cout << "VALOR OBJETIVO: " << objective << endl;

    system("pause");

    return 0;
}