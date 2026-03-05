
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

int main()
{
    string solutionFile = "solucao.csv";
    ifstream fin(solutionFile);
    if (!fin)
    {
        cerr << "Erro ao abrir arquivo de solução: " << solutionFile << endl;
        return 1;
    }

    // Lê cabeçalho: o, m, t, c
    string header;
    getline(fin, header);
    replace(header.begin(), header.end(), ',', ' ');
    stringstream sh(header);
    sh >> o >> m >> t >> c;

    std::vector<std::vector<Operation>> maquinas(m);
    string line;
    std::vector<Operation> vetOperacao;
    int i = 0;
    while (getline(fin, line) && !line.empty())
    {
        replace(line.begin(), line.end(), ',', ' ');
        stringstream ss(line);
        ss.imbue(locale("C"));

        int maq, idJob, idOp, toolSetId, toolSetSize;
        double releaseTime, processingTime, dueDate;

        ss >> maq >> idJob >> idOp >> releaseTime >> processingTime >> dueDate >> toolSetId >> toolSetSize;

        if (ss)
        {
            Operation op(i, idJob, idOp, toolSetId - 1, toolSetSize, processingTime, dueDate, releaseTime);
            maquinas[maq].push_back(op);
            vetOperacao.push_back(op);
            i++;
        }
    }
    fin.close();

    std::map<int, double> tempo_final;
    // Processa cada máquina

    double objective = objectiveFunction(maquinas, tempo_final, vetOperacao);

    cout << "VALOR OBJETIVO: " << objective << endl;

    return 0;
}