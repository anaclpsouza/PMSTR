#include "Buscas.h"
#include "Configuracao.h"
#include "ObjectiveFunction.h"
#include "Operation.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

int numMaquinas = 0;
int numOperacoes = 0;
int numConjuntosFerramentas = 0;
int capacidadeMagazine = 0;
std::vector<std::vector<Operation>> maquinas;

struct IdentificacaoSaida
{
    std::string cenario;
    std::string execucao;
    std::string instancia;
};

static void escreverMatrizFinalCompilada(const std::string &caminhoArquivo,
                                         const std::string &nomeInstancia,
                                         const std::string &execucao)
{
    std::ofstream out(caminhoArquivo, std::ios::app);
    if (!out.is_open())
        return;

    out << "Execucao: " << execucao << '\n';
    out << "Instancia: " << nomeInstancia << '\n';
    for (size_t maq = 0; maq < maquinas.size(); ++maq)
    {
        out << "M" << (maq + 1) << ":";
        for (const auto &op : maquinas[maq])
            out << " J" << op.idJob << "-O" << op.idOp;
        out << '\n';
    }
    out << '\n';
}

static int selecionarMaquinaMenorCarga(const std::vector<double> &cargaPorMaquina)
{
    int melhorMaq = 0;
    for (int maq = 1; maq < numMaquinas; ++maq)
    {
        if (cargaPorMaquina[maq] < cargaPorMaquina[melhorMaq])
            melhorMaq = maq;
    }
    return melhorMaq;
}

static void construirSolucaoAleatoriaPorJob(const std::map<int, std::deque<Operation>> &operacoesPorJob)
{
    std::vector<int> jobsDisponiveis;
    jobsDisponiveis.reserve(operacoesPorJob.size());
    for (const auto &[idJob, _] : operacoesPorJob)
        jobsDisponiveis.push_back(idJob);

    if (jobsDisponiveis.empty() || numMaquinas <= 0)
        return;

    std::mt19937 gerador(std::random_device{}());
    std::shuffle(jobsDisponiveis.begin(), jobsDisponiveis.end(), gerador);

    auto calcularCargaJob = [&](int idJob)
    {
        double soma = 0.0;
        for (const auto &op : operacoesPorJob.at(idJob))
            soma += op.releaseTime + op.processingTime;
        return soma;
    };

    std::vector<double> cargaPorMaquina(numMaquinas, 0.0);
    size_t idxJob = 0;

    const size_t alocacaoInicial = std::min(static_cast<size_t>(numMaquinas), jobsDisponiveis.size());
    for (size_t maq = 0; maq < alocacaoInicial; ++maq)
    {
        const int idJob = jobsDisponiveis[idxJob++];
        const auto &ops = operacoesPorJob.at(idJob);
        maquinas[maq].insert(maquinas[maq].end(), ops.begin(), ops.end());
        cargaPorMaquina[maq] += calcularCargaJob(idJob);
    }

    while (idxJob < jobsDisponiveis.size())
    {
        const int melhorMaq = selecionarMaquinaMenorCarga(cargaPorMaquina);
        const int idJob = jobsDisponiveis[idxJob++];
        const auto &ops = operacoesPorJob.at(idJob);
        maquinas[melhorMaq].insert(maquinas[melhorMaq].end(), ops.begin(), ops.end());
        cargaPorMaquina[melhorMaq] += calcularCargaJob(idJob);
    }
}

static void construirSolucaoEnviesada(std::vector<Operation> operacoes, bool priorizarDueDate)
{
    std::sort(operacoes.begin(), operacoes.end(), [priorizarDueDate](const Operation &a, const Operation &b)
              {
                  const double primarioA = priorizarDueDate ? a.dueDate : a.releaseTime;
                  const double primarioB = priorizarDueDate ? b.dueDate : b.releaseTime;
                  if (primarioA != primarioB)
                      return primarioA < primarioB;

                  const double secundarioA = priorizarDueDate ? a.releaseTime : a.dueDate;
                  const double secundarioB = priorizarDueDate ? b.releaseTime : b.dueDate;
                  if (secundarioA != secundarioB)
                      return secundarioA < secundarioB;

                  if (a.idJob != b.idJob)
                      return a.idJob < b.idJob;
                  return a.idOp < b.idOp;
              });

    std::vector<double> cargaProcessamento(numMaquinas, 0.0);
    std::map<int, int> maquinaPorJob;

    for (const auto &op : operacoes)
    {
        int maquinaEscolhida = -1;
        if (op.idOp > 1)
        {
            auto it = maquinaPorJob.find(op.idJob);
            if (it != maquinaPorJob.end())
                maquinaEscolhida = it->second;
        }

        if (maquinaEscolhida < 0)
            maquinaEscolhida = selecionarMaquinaMenorCarga(cargaProcessamento);

        maquinas[maquinaEscolhida].push_back(op);
        cargaProcessamento[maquinaEscolhida] += op.processingTime;
        maquinaPorJob[op.idJob] = maquinaEscolhida;
    }
}

static int extrairValorCabecalho(std::string linha)
{
    std::replace(linha.begin(), linha.end(), ',', ' ');
    std::stringstream ss(linha);
    ss.imbue(std::locale("C"));
    std::string chave;
    int valor = 0;
    ss >> chave >> valor;
    return valor;
}

static IdentificacaoSaida extrairIdentificacao(int argc, char *argv[], int maqs, int ops)
{
    const std::string instanciaPadrao = std::to_string(maqs) + "M" + std::to_string(ops);
    std::string execucao = "SAIDA_1";
    std::string nomeBaseInstancia;

    auto removerDiretorioEExtensao = [](std::string texto)
    {
        const size_t barra = texto.find_last_of("/\\");
        if (barra != std::string::npos)
            texto = texto.substr(barra + 1);
        if (texto.size() >= 4 && texto.substr(texto.size() - 4) == ".csv")
            texto = texto.substr(0, texto.size() - 4);
        return texto;
    };

    if (argc >= 5)
    {
        const std::string argExec = argv[4];
        execucao = (argExec.rfind("SAIDA_", 0) == 0) ? argExec : ("SAIDA_" + argExec);
        nomeBaseInstancia = removerDiretorioEExtensao(argv[3]);
    }
    else
    {
        const std::string nomeArquivoSaida = removerDiretorioEExtensao(argv[1]);
        if (nomeArquivoSaida.rfind("SAIDA_", 0) == 0)
        {
            const size_t segundoUnderscore = nomeArquivoSaida.find('_', 6);
            if (segundoUnderscore != std::string::npos)
            {
                execucao = nomeArquivoSaida.substr(0, segundoUnderscore);
                nomeBaseInstancia = nomeArquivoSaida.substr(segundoUnderscore + 1);
            }
            else
            {
                execucao = nomeArquivoSaida;
            }
        }
        else if (!nomeArquivoSaida.empty())
        {
            execucao = nomeArquivoSaida;
        }
    }

    std::string cenario = "-";
    if (!nomeBaseInstancia.empty())
    {
        const size_t posUnderscore = nomeBaseInstancia.find('_');
        cenario = (posUnderscore != std::string::npos && posUnderscore + 1 < nomeBaseInstancia.size())
                      ? nomeBaseInstancia.substr(posUnderscore + 1)
                      : nomeBaseInstancia;
    }

    return {cenario, execucao, instanciaPadrao};
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Arquivo de saida nao informado." << std::endl;
        return 1;
    }

    std::ios_base::sync_with_stdio(false);
    std::cin.imbue(std::locale("C"));

    std::string linha;
    if (std::getline(std::cin, linha))
        numOperacoes = extrairValorCabecalho(linha);
    if (std::getline(std::cin, linha))
        numMaquinas = extrairValorCabecalho(linha);
    if (std::getline(std::cin, linha))
        numConjuntosFerramentas = extrairValorCabecalho(linha);
    if (std::getline(std::cin, linha))
        capacidadeMagazine = extrairValorCabecalho(linha);

    // Descarta a linha em branco e o cabeçalho das colunas de operações.
    std::getline(std::cin, linha);
    std::getline(std::cin, linha);

    std::vector<Operation> operacoes;
    int idSequencial = 0;

    while (std::getline(std::cin, linha) && !linha.empty())
    {
        std::replace(linha.begin(), linha.end(), ',', ' ');
        std::stringstream ss(linha);
        ss.imbue(std::locale("C"));

        int idJob = 0;
        int idOp = 0;
        int toolSetId = 0;
        int toolSetSize = 0;
        double releaseTime = 0.0;
        double processingTime = 0.0;
        double dueDate = 0.0;

        ss >> idJob >> idOp >> releaseTime >> processingTime >> dueDate >> toolSetId >> toolSetSize;
        if (ss)
        {
            operacoes.emplace_back(idSequencial++, idJob, idOp, toolSetId - 1, toolSetSize, processingTime, dueDate, releaseTime);
        }
    }

    resetarEstatisticasKtns();
    inicioExecucao = std::chrono::high_resolution_clock::now();

    std::map<int, std::deque<Operation>> operacoesPorJob;
    for (const auto &op : operacoes)
        operacoesPorJob[op.idJob].push_back(op);

    double solucaoInicial = INT_MAX;
    std::vector<std::vector<Operation>> melhoresMaquinas;
    std::vector<double> melhorAtrasoPorMaquina;

    auto avaliarSolucaoInicial = [&](auto gerarSolucao, bool repetirAteValida)
    {
        double valor = INT_MAX;
        std::vector<double> atrasoCandidato;

        do
        {
            maquinas.clear();
            maquinas.resize(numMaquinas);
            gerarSolucao();
            valor = objectiveFunction(maquinas, operacoes, atrasoCandidato);
        } while (repetirAteValida && valor == INT_MAX);

        if (valor != INT_MAX && valor < solucaoInicial)
        {
            solucaoInicial = valor;
            melhoresMaquinas = maquinas;
            melhorAtrasoPorMaquina = std::move(atrasoCandidato);
        }
    };

    avaliarSolucaoInicial([&]() { construirSolucaoAleatoriaPorJob(operacoesPorJob); }, true);
    avaliarSolucaoInicial([&]() { construirSolucaoEnviesada(operacoes, false); }, false);
    avaliarSolucaoInicial([&]() { construirSolucaoEnviesada(operacoes, true); }, false);

    maquinas = std::move(melhoresMaquinas);
    std::vector<double> atrasoPorMaquina = std::move(melhorAtrasoPorMaquina);

    const auto params = Configuracao::obterParametros(numMaquinas);
    const double solucaoIls = ILS(params.reInsertion,
                                  params.insertionIm,
                                  params.twoSwap,
                                  params.limiteIteracoesSemMelhoria,
                                  params.percentualPerturbacao,
                                  params.limiteTempoHoras,
                                  maquinas,
                                  operacoes,
                                  atrasoPorMaquina,
                                  numOperacoes);

    const double tempoExecucaoSegundos =
        std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - inicioExecucao).count();

    std::ofstream arquivoSaida(argv[1]);
    if (!arquivoSaida.is_open())
    {
        std::cerr << "Nao foi possivel criar o arquivo de saida: " << argv[1] << std::endl;
        return 1;
    }

    const auto idSaida = extrairIdentificacao(argc, argv, numMaquinas, numOperacoes);

    arquivoSaida
        << "Cenario,Execucao,Instancia,O,M,T,C,Solucao_Inicial,ILS,Tempo_Execucao(s),KTNS_Antigo,KTNS_Novo,Empates\n"
        << idSaida.cenario << ","
        << idSaida.execucao << ","
        << idSaida.instancia << ","
        << numOperacoes << ","
        << numMaquinas << ","
        << numConjuntosFerramentas << ","
        << capacidadeMagazine << ","
        << solucaoInicial << ","
        << solucaoIls << ","
        << tempoExecucaoSegundos << ","
        << estatisticasKtns.ktnsAntigo << ","
        << estatisticasKtns.ktnsNovo << ","
        << estatisticasKtns.empates << '\n';

    if (argc >= 5)
        escreverMatrizFinalCompilada(argv[2], argv[3], argv[4]);

    return 0;
}
