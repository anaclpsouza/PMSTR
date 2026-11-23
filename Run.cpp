#include "Configuracao.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>

static void garantirDiretorioSolucoes(const std::string &caminhoSolucoes)
{
    struct stat statusDir{};
    if (stat(caminhoSolucoes.c_str(), &statusDir) != -1)
        return;

    if (mkdir(caminhoSolucoes.c_str(), 0755) != 0)
        std::cerr << "Falha ao criar o diretorio '" << caminhoSolucoes << "': " << std::strerror(errno) << std::endl;
    else
        std::cout << "Diretorio criado: " << caminhoSolucoes << std::endl;
}

static void garantirCabecalhoMatrizes(const std::string &caminhoMatrizes)
{
    struct stat statusArquivo{};
    if (stat(caminhoMatrizes.c_str(), &statusArquivo) == 0 && statusArquivo.st_size > 0)
        return;

    std::ofstream arquivoMatrizes(caminhoMatrizes, std::ios::trunc);
    if (!arquivoMatrizes.is_open())
    {
        std::cerr << "Falha ao criar arquivo compilado de matrizes: " << caminhoMatrizes << std::endl;
        return;
    }

    arquivoMatrizes << "MATRIZES FINAIS COMPILADAS\n"
                    << "=========================\n\n";
}

static void extrairIdentificacaoLegada(const std::string &nomeArquivo,
                                       std::string &outExecucao,
                                       std::string &outCenario)
{
    std::string base = nomeArquivo;
    if (base.size() >= 4 && base.substr(base.size() - 4) == ".csv")
        base = base.substr(0, base.size() - 4);

    outExecucao = base;
    outCenario = "-";

    const size_t segundoUnderscore = base.find('_', 6);
    if (segundoUnderscore == std::string::npos)
        return;

    outExecucao = base.substr(0, segundoUnderscore);
    const std::string resto = base.substr(segundoUnderscore + 1);
    const size_t underscoreCenario = resto.find('_');
    outCenario = (underscoreCenario != std::string::npos && underscoreCenario + 1 < resto.size())
                     ? resto.substr(underscoreCenario + 1)
                     : resto;
}

static void gerarResumoCsv(const std::string &diretorioInstancias,
                           const std::string &caminhoSolucoes,
                           const std::string &caminhoMatrizes)
{
    std::cout << "Generating summary in: " << caminhoSolucoes << std::endl;

    DIR *dirSolucoes = opendir(caminhoSolucoes.c_str());
    if (dirSolucoes == nullptr)
    {
        std::cerr << "Could not open solucoes directory for summary: " << std::strerror(errno) << std::endl;
        return;
    }

    const std::string caminhoResumo = caminhoSolucoes + "/RESUMO.csv";
    std::ofstream arquivoResumo(caminhoResumo);
    if (!arquivoResumo.is_open())
    {
        std::cerr << "Failed to open resumo file for writing: " << caminhoResumo << std::endl;
        closedir(dirSolucoes);
        return;
    }

    const int numMaqConfig = (diretorioInstancias.find("2M") != std::string::npos) ? 2 : 6;
    const auto params = Configuracao::obterParametros(numMaqConfig);

    arquivoResumo << "RE_INSERTION,INSERTION_IM,TWO_SWAP,LIMITE_SEM_MELHORIA,PERCENTUAL_PERTURBACAO,LIMITE_TEMPO_HORAS\n"
                  << params.reInsertion << ","
                  << params.insertionIm << ","
                  << params.twoSwap << ","
                  << params.limiteIteracoesSemMelhoria << ","
                  << params.percentualPerturbacao << ","
                  << params.limiteTempoHoras << "\n\n"
                  << "Cenario,Execucao,Instancia,O,M,T,C,Solucao_Inicial,ILS,Tempo_Execucao(s),KTNS_Antigo,KTNS_Novo,Empates\n";

    struct dirent *entradaResumo = nullptr;
    while ((entradaResumo = readdir(dirSolucoes)) != nullptr)
    {
        if (entradaResumo->d_type != DT_REG)
            continue;

        const std::string nomeArquivo = entradaResumo->d_name;
        if (nomeArquivo == "RESUMO.csv" || nomeArquivo.rfind("SAIDA_", 0) != 0)
            continue;

        std::ifstream arquivoSaida(caminhoSolucoes + "/" + nomeArquivo);
        if (!arquivoSaida.is_open())
            continue;

        std::string linha;
        if (!std::getline(arquivoSaida, linha))
        {
            std::cout << "Não leu as linhas" << std::endl;
            continue;
        }
        if (!std::getline(arquivoSaida, linha))
            continue;

        std::replace(linha.begin(), linha.end(), ',', ' ');
        std::istringstream iss(linha);

        std::string cenario, execucao, instancia, solucaoInicial, ils, tempoExecucao;
        int numOps = 0, numMaqs = 0, numConjuntos = 0, capacidade = 0;
        long long ktnsAntigo = 0, ktnsNovo = 0, empates = 0;

        if (iss >> cenario >> execucao >> instancia >> numOps >> numMaqs >> numConjuntos >> capacidade >> solucaoInicial >> ils >> tempoExecucao >> ktnsAntigo >> ktnsNovo >> empates)
        {
            arquivoResumo << cenario << ","
                          << execucao << ","
                          << instancia << ","
                          << numOps << ","
                          << numMaqs << ","
                          << numConjuntos << ","
                          << capacidade << ","
                          << solucaoInicial << ","
                          << ils << ","
                          << tempoExecucao << ","
                          << ktnsAntigo << ","
                          << ktnsNovo << ","
                          << empates << '\n';
            continue;
        }

        // Compatibilidade com arquivos SAIDA_* legados de 8 colunas.
        std::istringstream issLegado(linha);
        if (issLegado >> instancia >> numOps >> numMaqs >> numConjuntos >> capacidade >> solucaoInicial >> ils >> tempoExecucao)
        {
            extrairIdentificacaoLegada(nomeArquivo, execucao, cenario);
            arquivoResumo << cenario << ","
                          << execucao << ","
                          << instancia << ","
                          << numOps << ","
                          << numMaqs << ","
                          << numConjuntos << ","
                          << capacidade << ","
                          << solucaoInicial << ","
                          << ils << ","
                          << tempoExecucao << ",0,0,0\n";
        }
    }

    closedir(dirSolucoes);
    std::cout << "Summary written to: " << caminhoResumo << std::endl;
    std::cout << "Final matrices written to: " << caminhoMatrizes << std::endl;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Diretório com as instâncias não foi informado." << std::endl;
        return 1;
    }

    std::string diretorioInstancias = argv[1];
    const int repeticoes = std::stoi(argv[2]);

    if (!diretorioInstancias.empty() && diretorioInstancias.back() != '/')
        diretorioInstancias.push_back('/');

    DIR *dirInstancias = opendir(diretorioInstancias.c_str());
    if (dirInstancias == nullptr)
    {
        std::cerr << "Nao foi possível abrir diretorio com as instâncias." << std::endl;
        return 1;
    }

    const std::string caminhoSolucoes = diretorioInstancias + "solucoes";
    const std::string caminhoMatrizes = caminhoSolucoes + "/MATRIZES_FINAIS.txt";

    garantirDiretorioSolucoes(caminhoSolucoes);
    garantirCabecalhoMatrizes(caminhoMatrizes);

    struct dirent *entradaInstancia = nullptr;
    while ((entradaInstancia = readdir(dirInstancias)) != nullptr)
    {
        if (entradaInstancia->d_type != DT_REG)
            continue;

        const std::string nomeInstancia = entradaInstancia->d_name;

        for (int exec = 1; exec <= repeticoes; ++exec)
        {
            const std::string numExec = std::to_string(exec);
            std::cout << exec << " Execucao: " << diretorioInstancias << nomeInstancia << std::endl;

            const std::string arquivoSaida = caminhoSolucoes + "/SAIDA_" + numExec + "_" + nomeInstancia;
            struct stat statusSaida{};
            if (stat(arquivoSaida.c_str(), &statusSaida) == 0)
            {
                if (statusSaida.st_size > 0)
                {
                    std::cout << "Essa execução terminou (size=" << statusSaida.st_size << "). Pulando para a próxima." << std::endl;
                    continue;
                }
                std::cout << "Arquivo de saída vazio detectado (size=0). Reexecutando: " << arquivoSaida << std::endl;
            }

            const std::string cmd = "./Main \"" + arquivoSaida + "\" \"" + caminhoMatrizes + "\" \"" +
                                    nomeInstancia + "\" \"" + numExec + "\" <\"" +
                                    diretorioInstancias + nomeInstancia + "\"";
            std::cout << "Running: " << cmd << std::endl;

            const int codigoRetorno = std::system(cmd.c_str());
            std::cout << "Return code: " << codigoRetorno << " for output file: " << arquivoSaida << std::endl;

            if (stat(arquivoSaida.c_str(), &statusSaida) == 0)
                std::cout << "Output created: " << arquivoSaida << " (size=" << statusSaida.st_size << ")" << std::endl;
            else
                std::cout << "Output NOT found: " << arquivoSaida << " after running main" << std::endl;
        }
    }
    closedir(dirInstancias);

    gerarResumoCsv(diretorioInstancias, caminhoSolucoes, caminhoMatrizes);
    return 0;
}
