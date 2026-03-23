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

static bool get_num(const std::string &s, const std::string &key, double &out)
{
	size_t p = s.find(key);
	if (p == std::string::npos)
		return false;
	p += key.size();
	size_t e = s.find_first_of(" \t\r\n", p);
	std::string v = s.substr(p, (e == std::string::npos) ? std::string::npos : (e - p));
	try
	{
		out = std::stod(v);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

static bool get_tail(const std::string &s, const std::string &key, std::string &out)
{
	size_t p = s.find(key);
	if (p == std::string::npos)
		return false;
	p += key.size();
	while (p < s.size() && (s[p] == ' ' || s[p] == '\t'))
		++p;
	out = s.substr(p);
	while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ' || out.back() == '\t'))
		out.pop_back();
	return true;
}

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
					std::cout << "Essa execução terminou. Pulando para a próxima." << std::endl;
					continue;
				}
				std::string cmd = "./Main " + outFile + " <" + nomeDir + entrada->d_name;
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
			fileR << "Arquivo,Instancia,O,M,T,C,Solucao_Inicial,Tempo_Execucao(s)" << std::endl;

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
					std::string instance, inicial_sol, tempo_sol;
					int o, m, t, c;
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
					if (iss >> instance >> o >> m >> t >> c >> inicial_sol >> tempo_sol)
					{
						fileR << nomeArq
							  << "," << instance
							  << "," << o
							  << "," << m
							  << "," << t
							  << "," << c
							  << "," << inicial_sol
							  << "," << tempo_sol
							  << std::endl;
					}
					file.close();
				}
			}
			fileR.close();
			std::cout << "Summary written to: " << resumoPath << std::endl;
		}
		closedir(dir2);
	}
	return 0;
}
