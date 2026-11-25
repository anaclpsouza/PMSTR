# Resultados Experimentais

Esta pasta reúne apenas os arquivos de resumo (`RESUMO.csv`) da execução de referência correspondente à configuração calibrada abaixo, além das 3 tabelas consolidadas por cenário (`RESULTADOS_CONSOLIDADOS_SCENARIO1.csv`, `RESULTADOS_CONSOLIDADOS_SCENARIO2.csv` e `RESULTADOS_CONSOLIDADOS_SCENARIO3.csv`).

## Referências dos Métodos Comparados

- **MH (Matheuristic):** Dang, Q.-V., van Diessen, T., Martagan, T. G., & Adan, I. J. B. F. (2021). *"[A matheuristic for parallel machine scheduling with tool replacements](https://doi.org/10.1016/j.ejor.2020.09.050)"*. European Journal of Operational Research, 291(2), 640–660.
- **SSGA (Steady-State Genetic Algorithm):** Souza et al. (2026).

## Configuração Utilizada (ILS)

| Parâmetro | 2 Máquinas (`2M38`, `2M46`) | 6 Máquinas (`6M140`, `6M163`) |
| :--- | :---: | :---: |
| `RE_INSERTION` | `1` | `0` |
| `INSERTION_IM` | `1` | `1` |
| `TWO_SWAP` | `0` | `1` |
| `LIMITE_ITERACOES_SEM_MELHORIA` | `100` | `300` |
| `PERCENTUAL_PERTURBACAO` | `0.02` | `0.01` |
| `LIMITE_TEMPO_HORAS` | `2` | `2` |

## Estrutura dos Arquivos

- [`RESULTADOS_CONSOLIDADOS_SCENARIO1.csv`](RESULTADOS_CONSOLIDADOS_SCENARIO1.csv): consolidado do **Cenário 1** (variação do fator de *release time* $R$ e do número de conjuntos de ferramentas $T$), comparando **MH**, **SSGA** e **ILS**, além do *gap* em relação a `MIN(SSGA-MH)` e da taxa de escolha entre **KTNS Antigo** e **KTNS Novo**.
- [`RESULTADOS_CONSOLIDADOS_SCENARIO2.csv`](RESULTADOS_CONSOLIDADOS_SCENARIO2.csv): consolidado do **Cenário 2** (variação da capacidade do magazine $C$: `53`, `65`, `80`, `95`, `110`).
- [`RESULTADOS_CONSOLIDADOS_SCENARIO3.csv`](RESULTADOS_CONSOLIDADOS_SCENARIO3.csv): consolidado do **Cenário 3** (variação das *due dates* $D$).
- `<Instancia>/Scenario{1,2,3}/RESUMO.csv`: resumos individuais de cada cenário (`2M38`, `2M46`, `6M140`, `6M163`), com os parâmetros da instância (`Instancia,O,M,T,C`) no cabeçalho e 50 execuções por arquivo (10 repetições por variação). Os arquivos `SAIDA_*.csv` e `MATRIZES_FINAIS.txt` não são versionados aqui.
