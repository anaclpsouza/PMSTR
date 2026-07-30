#ifndef CONFIGURACAO_H
#define CONFIGURACAO_H


namespace Configuracao
{
    // 1 = habilita a busca local; 0 = desabilita.
    inline constexpr int RE_INSERTION = 1;
    inline constexpr int INSERTION_IM = 1;
    inline constexpr int TWO_SWAP = 1;

    inline constexpr int LIMITE_ITERACOES_SEM_MELHORIA = 100;

    inline constexpr double PERCENTUAL_PERTURBACAO = 0.02;

    inline constexpr int LIMITE_TEMPO_HORAS = 2;
}


#endif
