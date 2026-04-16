
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
#include "Buscas.h"

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
        tarefas[op.idJob].push_back(op);
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
        int idSorteado = dis(g);
        int idJobSorteado = tarefasDisponiveis[idSorteado];

        auto &fila = tarefas[idJobSorteado];
        operacoesRandomizadas.push_back(fila.front());
        fila.pop_front();

        if (fila.empty())
        {
            tarefasDisponiveis.erase(tarefasDisponiveis.begin() + idSorteado);
        }
    }

    return operacoesRandomizadas;
}

void atribuirMaquinas(std::vector<Operation> operacoes)
{
    if (true)
    {
        cout << "[DEBUG] Gerando solução inicial aleatória" << endl;
    }

    std::vector<double> tempoMaq(m, 0.0);
    std::map<int, double> tempoJob;

    for (const auto &op : operacoes)
    {
        int melhorMaq = 0;
        double menorTermino = -1.0;

        for (int j = 0; j < m; j++)
        {
            double prontoJob = (op.idOp > 1) ? tempoJob[op.idJob] : 0.0;

            double inicioPossivel = std::max({tempoMaq[j], prontoJob, (double)op.releaseTime});
            double terminoPrevisto = inicioPossivel + op.processingTime;

            if (menorTermino < 0 || terminoPrevisto < menorTermino)
            {
                menorTermino = terminoPrevisto;
                melhorMaq = j;
            }
        }

        maquinas[melhorMaq].push_back(op);

        double prontoJob = (op.idOp > 1) ? tempoJob[op.idJob] : 0.0;
        double inicioReal = std::max({tempoMaq[melhorMaq], prontoJob, (double)op.releaseTime});

        tempoMaq[melhorMaq] = inicioReal + op.processingTime;
        tempoJob[op.idJob] = tempoMaq[melhorMaq];
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

   /*  maquinas.clear();
    maquinas.resize(m); */
    /*  std::vector<Operation> opsAleatorias = randomizarOp(vetOperacao);
     atribuirMaquinas(opsAleatorias);
  */
    std::map<int, double> tempo_final;
    std::vector<double> tardiness_maq;
    std::vector<Operation> opsAleatorias;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    double sol_inicial = INT_MAX;
    while (sol_inicial == INT_MAX)
    {
        maquinas.clear();
        maquinas.resize(m);
        opsAleatorias = randomizarOp(vetOperacao);
        atribuirMaquinas(opsAleatorias);
        sol_inicial = objectiveFunction(maquinas, vetOperacao, controleOp, tardiness_maq);
    }

    double sol_insertion = re_insertion(maquinas, vetOperacao, controleOp, tardiness_maq);
    double sol_insertionIM = insertion_im(maquinas, vetOperacao, controleOp, tardiness_maq);
    double sol_twoSwap = two_swap(maquinas, vetOperacao, controleOp, tardiness_maq);

    high_resolution_clock::time_point t2 = high_resolution_clock::now();

    auto tempo_execucao = duration_cast<duration<double>>(t2 - t1);

    fileSolution
        << "Instance_name,O,M,T,C,Solucao_Inicial,Insertion,InsertionIM,2Swap,Tempo de_execucao(s)" << endl
        << m << "M" << o << ","
        << o << ","
        << m << ","
        << t << ","
        << c << ","
        << sol_inicial << ","
        << sol_insertion << ","
        << sol_insertionIM << ","
        << sol_twoSwap << ","
        << tempo_execucao.count() << endl;

    fileSolution.close();

    return 0;
}