#include <dirent.h>
#include <cstdlib>
#include <string>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "Configuracao.h"

int main(int argc, char *argv[])
{
	if (argc != 3)
	{
		std::cerr << "Diretório com as instâncias não foi informado." << std::endl;
		exit(1);
	}
	std::string nomeDir = argv[1];
	int repeticoes = std::stoi(argv[2]);

	if (!nomeDir.empty() && nomeDir.back() != '/')
		nomeDir.push_back('/');
	DIR *dir = 0;
	struct dirent *entrada = 0;
	unsigned char isFile = 0x8;

	dir = opendir(nomeDir.c_str());

	if (dir == 0)
	{
		std::cerr << "Nao foi possível abrir diretorio com as instâncias." << std::endl;
		exit(1);
	}

	std::string solucoesPath = nomeDir;
	if (!solucoesPath.empty() && solucoesPath.back() != '/')
		solucoesPath.push_back('/');
	solucoesPath += "solucoes";
	std::string matrizPath = solucoesPath + "/MATRIZES_FINAIS.txt";

	struct stat st;
	if (stat(solucoesPath.c_str(), &st) == -1)
	{
		if (mkdir(solucoesPath.c_str(), 0755) != 0)
		{
			std::cerr << "Falha ao criar o diretório '" << solucoesPath << "': " << strerror(errno) << std::endl;
		}
		else
		{
			std::cout << "Diretório criado: " << solucoesPath << std::endl;
		}
	}

	struct stat st_mat;
	if (stat(matrizPath.c_str(), &st_mat) != 0 || st_mat.st_size == 0)
	{
		std::ofstream fileM(matrizPath.c_str(), std::ios::trunc);
		if (!fileM.is_open())
		{
			std::cerr << "Falha ao criar arquivo compilado de matrizes: " << matrizPath << std::endl;
		}
		else
		{
			fileM << "MATRIZES FINAIS COMPILADAS" << std::endl;
			fileM << "=========================" << std::endl
				  << std::endl;
		}
	}

	while ((entrada = readdir(dir)))
	{
		if (entrada->d_type == isFile)
		{
			for (int i = 1; i <= repeticoes; ++i)
			{
				std::stringstream convert;
				std::cout << i << " Execucao: " << nomeDir + entrada->d_name << std::endl;
				convert << i;
				std::string outFile = nomeDir + "solucoes/SAIDA_" + convert.str() + "_" + entrada->d_name;
				struct stat st_out;
				if (stat(outFile.c_str(), &st_out) == 0)
				{
					if (st_out.st_size > 0)
					{
						std::cout << "Essa execução terminou (size=" << st_out.st_size << "). Pulando para a próxima." << std::endl;
						continue;
					}
					else
					{
						std::cout << "Arquivo de saída vazio detectado (size=0). Reexecutando: " << outFile << std::endl;
					}
				}
				std::string cmd = "./Main \"" + outFile + "\" \"" + matrizPath + "\" \"" + entrada->d_name + "\" \"" + convert.str() + "\" <\"" + nomeDir + entrada->d_name + "\"";
				std::cout << "Running: " << cmd << std::endl;
				int s = system(cmd.c_str());
				std::cout << "Return code: " << s << " for output file: " << outFile << std::endl;

				if (stat(outFile.c_str(), &st_out) == 0)
				{
					std::cout << "Output created: " << outFile << " (size=" << st_out.st_size << ")" << std::endl;
				}
				else
				{
					std::cout << "Output NOT found: " << outFile << " after running main" << std::endl;
				}
			}
		}
	}
	closedir(dir);

	std::cout << "Generating summary in: " << solucoesPath << std::endl;
	DIR *dir2 = opendir(solucoesPath.c_str());
	if (dir2 == nullptr)
	{
		std::cerr << "Could not open solucoes directory for summary: " << strerror(errno) << std::endl;
	}
	else
	{
		std::ifstream file;
		std::ofstream fileR;
		std::string resumoPath = solucoesPath + "/RESUMO.csv";
		fileR.open(resumoPath.c_str());
		if (!fileR.is_open())
		{
			std::cerr << "Failed to open resumo file for writing: " << resumoPath << std::endl;
		}
		else
		{
			const int numMaqConfig = (nomeDir.find("2M") != std::string::npos) ? 2 : 6;
			const auto params = Configuracao::obterParametros(numMaqConfig);

			fileR << "RE_INSERTION,INSERTION_IM,TWO_SWAP,LIMITE_SEM_MELHORIA,PERCENTUAL_PERTURBACAO,LIMITE_TEMPO_HORAS" << std::endl;
			fileR << params.reInsertion << ","
				  << params.insertionIm << ","
				  << params.twoSwap << ","
				  << params.limiteIteracoesSemMelhoria << ","
				  << params.percentualPerturbacao << ","
				  << params.limiteTempoHoras << std::endl;
			fileR << std::endl;
			fileR << "Cenario,Execucao,Instancia,O,M,T,C,Solucao_Inicial,ILS,Tempo_Execucao(s),KTNS_Antigo,KTNS_Novo,Empates" << std::endl;

			struct dirent *ent2;
			while ((ent2 = readdir(dir2)) != nullptr)
			{
				if (ent2->d_type == isFile)
				{
					std::string nomeArq = ent2->d_name;
					if (nomeArq == "RESUMO.csv" || nomeArq.rfind("SAIDA_", 0) != 0)
						continue;
					std::string filePath = solucoesPath + "/" + nomeArq;
					file.open(filePath.c_str(), std::ifstream::in);
					if (!file.is_open())
						continue;

					std::string line;
					std::string cenario, execucao, instance, inicial_sol, tempo_sol, ils;
					int o, m, t, c;
					long long ktns_antigo = 0, ktns_novo = 0, empates = 0;
					if (!std::getline(file, line))
					{
						file.close();
						std::cout << "Não leu as linhas" << std::endl;
						continue;
					}

					if (!std::getline(file, line)) // linha de dados
					{
						file.close();
						continue;
					}

					std::replace(line.begin(), line.end(), ',', ' ');
					std::istringstream iss(line);
					if (iss >> cenario >> execucao >> instance >> o >> m >> t >> c >> inicial_sol >> ils >> tempo_sol >> ktns_antigo >> ktns_novo >> empates)
					{
						fileR << cenario
							  << "," << execucao
							  << "," << instance
							  << "," << o
							  << "," << m
							  << "," << t
							  << "," << c
							  << "," << inicial_sol
							  << "," << ils
							  << "," << tempo_sol
							  << "," << ktns_antigo
							  << "," << ktns_novo
							  << "," << empates
							  << std::endl;
					}
					else
					{
						// Compatibilidade caso exista algum arquivo SAIDA_* antigo de 8 colunas
						std::istringstream issLegacy(line);
						if (issLegacy >> instance >> o >> m >> t >> c >> inicial_sol >> ils >> tempo_sol)
						{
							std::string base = nomeArq;
							if (base.size() >= 4 && base.substr(base.size() - 4) == ".csv")
								base = base.substr(0, base.size() - 4);
							execucao = base;
							cenario = "-";
							size_t segUnd = base.find('_', 6);
							if (segUnd != std::string::npos)
							{
								execucao = base.substr(0, segUnd);
								std::string resto = base.substr(segUnd + 1);
								size_t undCen = resto.find('_');
								cenario = (undCen != std::string::npos && undCen + 1 < resto.size())
											  ? resto.substr(undCen + 1)
											  : resto;
							}
							fileR << cenario
								  << "," << execucao
								  << "," << instance
								  << "," << o
								  << "," << m
								  << "," << t
								  << "," << c
								  << "," << inicial_sol
								  << "," << ils
								  << "," << tempo_sol
								  << ",0,0,0"
								  << std::endl;
						}
					}
					file.close();
				}
			}
			fileR.close();
			std::cout << "Summary written to: " << resumoPath << std::endl;
			std::cout << "Final matrices written to: " << matrizPath << std::endl;
		}
		closedir(dir2);
	}
	return 0;
}
