
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>
#include "Operation.h"
#include "ObjectiveFunction.h"

using namespace std;
int m = 0;
int o = 0;
int t = 0;
int c = 0;

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

int main()
{
    string solutionFile = "2M38.csv";
    ifstream fin(solutionFile);
    if (!fin)
    {
        cerr << "Erro ao abrir arquivo de solução: " << solutionFile << endl;
        return 1;
    }

    ios_base::sync_with_stdio(false);
    fin.tie(NULL);
    fin.imbue(locale("C"));

    string line;

    if (getline(fin, line))
        o = parseHeaderValue(line);
    if (getline(fin, line))
        m = parseHeaderValue(line);
    if (getline(fin, line))
        t = parseHeaderValue(line);
    if (getline(fin, line))
        c = parseHeaderValue(line);

    getline(fin, line);
    getline(fin, line);

    std::vector<std::vector<Operation>> maquinas(m);
    std::vector<Operation> vetOperacao;
    int i = 0;
    int operacao_global = 0;
    while (getline(fin, line) && !line.empty())
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
            i++;
            operacao_global++;
        }
    }
    fin.close();

    std::map<int, double> tempo_final;

    double objective = objectiveFunction(maquinas, tempo_final, vetOperacao);

    cout << "VALOR OBJETIVO: " << objective << endl;

    return 0;
}