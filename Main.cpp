
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

using namespace std;
int m = 0;
int o = 0;
int t = 0;
int c = 0;
std::ofstream fileSolution;
using namespace std::chrono;

int parseHeaderValue(string line)
{
    replace(line.begin(), line.end(), ',', ' ');
    stringstream ss(line);
    ss.imbue(locale("C")); // Força ponto decimal
    string key;
    int value;
    ss >> key >> value;
    return value;
}

int main(int argsc, char* argv[])
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

    std::vector<std::vector<Operation>> maquinas(m);
    std::vector<Operation> vetOperacao;
    int i = 0;
    int operacao_global = 0;
    std::map<int, std::map<int, int>> controleOp;
   
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
            int maq = operacao_global % m; // round-robin
            maquinas[maq].push_back(op);
            vetOperacao.push_back(op);
            controleOp[idJob][idOp] = 0;
            i++;
            operacao_global++;
        }
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