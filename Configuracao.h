#ifndef CONFIGURACAO_H
#define CONFIGURACAO_H

#include <cstdlib>

namespace Configuracao
{
    // Flag da versão do KTNS:
    // 0 = Versão antiga (KTNS original: remoção por maior distância de reuso, sem desempate por tamanho)
    // 1 = Versão nova (KTNS adaptado: remoção por maior tamanho com desempate pela maior distância de reuso)
    inline constexpr int KTNS_POR_TAMANHO = 1;

    // Permite sobrescrever via variável de ambiente PMSTR_KTNS_POR_TAMANHO ("0" ou "1") em baterias comparativas
    inline int modoKtnsAtivo()
    {
        const char *env = std::getenv("PMSTR_KTNS_POR_TAMANHO");
        if (env != nullptr && env[0] != '\0')
        {
            return (env[0] == '0') ? 0 : 1;
        }
        return KTNS_POR_TAMANHO;
    }

    // Parâmetros calibrados para 2 máquinas
    inline constexpr int RE_INSERTION_2M = 1;
    inline constexpr int INSERTION_IM_2M = 1;
    inline constexpr int TWO_SWAP_2M = 0;
    inline constexpr int LIMITE_ITERACOES_SEM_MELHORIA_2M = 100;
    inline constexpr double PERCENTUAL_PERTURBACAO_2M = 0.02;
    inline constexpr int LIMITE_TEMPO_HORAS_2M = 2;

    // Parâmetros calibrados para 6 máquinas (valores padrão anteriores)
    inline constexpr int RE_INSERTION_6M = 0;
    inline constexpr int INSERTION_IM_6M = 1;
    inline constexpr int TWO_SWAP_6M = 1;
    inline constexpr int LIMITE_ITERACOES_SEM_MELHORIA_6M = 300;
    inline constexpr double PERCENTUAL_PERTURBACAO_6M = 0.01;
    inline constexpr int LIMITE_TEMPO_HORAS_6M = 2;

    // Mantém aliases padrão (6 máquinas) para compatibilidade
    inline constexpr int RE_INSERTION = RE_INSERTION_6M;
    inline constexpr int INSERTION_IM = INSERTION_IM_6M;
    inline constexpr int TWO_SWAP = TWO_SWAP_6M;
    inline constexpr int LIMITE_ITERACOES_SEM_MELHORIA = LIMITE_ITERACOES_SEM_MELHORIA_6M;
    inline constexpr double PERCENTUAL_PERTURBACAO = PERCENTUAL_PERTURBACAO_6M;
    inline constexpr int LIMITE_TEMPO_HORAS = LIMITE_TEMPO_HORAS_6M;

    struct ParametrosILS
    {
        int reInsertion;
        int insertionIm;
        int twoSwap;
        int limiteIteracoesSemMelhoria;
        double percentualPerturbacao;
        int limiteTempoHoras;
    };

    inline constexpr ParametrosILS obterParametros(int numMaquinas)
    {
        if (numMaquinas == 2)
        {
            return {
                RE_INSERTION_2M,
                INSERTION_IM_2M,
                TWO_SWAP_2M,
                LIMITE_ITERACOES_SEM_MELHORIA_2M,
                PERCENTUAL_PERTURBACAO_2M,
                LIMITE_TEMPO_HORAS_2M};
        }
        return {
            RE_INSERTION_6M,
            INSERTION_IM_6M,
            TWO_SWAP_6M,
            LIMITE_ITERACOES_SEM_MELHORIA_6M,
            PERCENTUAL_PERTURBACAO_6M,
            LIMITE_TEMPO_HORAS_6M};
    }
}

#endif
