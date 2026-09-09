# PMSTR — ILS para Escalonamento de Múltiplas Máquinas com Restrição de Ferramentas

Implementação de uma **Iterated Local Search (ILS)** para o problema de escalonamento em máquinas paralelas com trocas de ferramentas (Parallel Machine Scheduling with Tool Replacements — PMSTR), desenvolvida como trabalho de conclusão de curso.

> O problema e suas instâncias são descritos em detalhes no artigo:  
> *"[A matheuristic for parallel machine scheduling with tool replacements](https://doi.org/10.1016/j.ejor.2020.09.050)"*.  
> O artigo aborda o mesmo problema, porém com uma abordagem matheurística (programação matemática + heurística). Este projeto implementa uma alternativa puramente heurística baseada em ILS com VND.

---

## O Problema

O PMSTR consiste em alocar e sequenciar um conjunto de **operações** (agrupadas em *jobs*) em **máquinas paralelas**, respeitando:

- **Release time** — instante mais cedo em que uma operação pode começar.
- **Due date** — prazo de entrega desejado.
- **Restrição de ferramentas** — cada operação requer um conjunto de ferramentas específico. A capacidade do magazine da máquina é limitada (`C`), podendo ser necessário realizar trocas de ferramentas entre operações consecutivas, o que impacta o makespan e o atraso.

A **função objetivo** minimiza o **total de tardiness** (soma dos atrasos) de todas as operações.

---

## A Abordagem: Iterated Local Search (ILS)

A ILS é uma metaheurística que itera entre **perturbação** e **intensificação** para escapar de ótimos locais e explorar o espaço de soluções de forma eficiente.

### Fluxo Geral

```
Solução Inicial
      │
      ▼
Intensificação (VND com as 3 buscas locais)
      │
      ▼
┌─────────────────────────────────────────┐
│  Perturbação                            │
│       │                                 │
│       ▼                                 │
│  VND (Re-insertion → Insertion-IM →     │  ◄── repete até critério de parada
│        Two-Swap)                        │
│       │                                 │
│       ▼                                 │
│  Aceita como nova base?                 │
│  (critério: custo ≤ 110% do melhor)     │
└─────────────────────────────────────────┘
      │
      ▼
Melhor solução encontrada
```

**Critérios de parada:** número máximo de iterações consecutivas sem melhoria global **ou** limite de tempo (configurável em `Configuracao.h`).

### Solução Inicial

Três heurísticas construtivas são executadas e a melhor é selecionada:

1. **Distribuição aleatória por tarefa** — embaralha os *jobs* e os distribui às máquinas pela menor carga acumulada (repete até gerar solução viável).
2. **Enviesada por release time** — ordena operações pelo release time crescente e as aloca na máquina de menor carga, mantendo todas as operações de um mesmo job na mesma máquina.
3. **Enviesada por due date** — idem ao anterior, mas ordena pelo due date.

### Perturbação

A perturbação realiza **trocas aleatórias entre máquinas** (*inter-machine swap*): seleciona `k` pares de operações em máquinas distintas e as troca de posição, diversificando a solução base antes de cada fase de intensificação. O número de trocas `k` é proporcional ao total de operações (configurável via `PERCENTUAL_PERTURBACAO`).

### Buscas Locais (VND)

As três buscas locais são executadas em sequência dentro de um **Variable Neighborhood Descent (VND)**: ao encontrar melhoria em qualquer busca, reinicia pelo primeiro vizinho. Todas adotam a estratégia **First Improvement**.

| Busca | Descrição |
|---|---|
| **Re-insertion** | Remove uma operação da máquina com maior atraso e a reinsere em outra posição da **mesma máquina**, buscando um sequenciamento melhor. |
| **Insertion-IM** | Remove operações da máquina mais atrasada e tenta inseri-las em posições de **outras máquinas** (inter-machine), buscando redistribuição de carga. |
| **Two-Swap** | Troca a posição de dois pares de operações dentro da **mesma máquina**, na ordem de máquina com maior atraso primeiro. |

---

## Estrutura de Arquivos

```
PMSTR/
├── Main.cpp               # Ponto de entrada: leitura de instância, geração de solução
│                          #   inicial e chamada da ILS
├── Buscas.cpp             # Implementação da ILS, das 3 buscas locais e da perturbação
├── Buscas.h               # Declarações das buscas e da ILS
├── ObjectiveFunction.cpp  # Cálculo do valor objetivo (total tardiness com trocas de ferramentas)
├── ObjectiveFunction.h    # Declaração da função objetivo
├── Operation.h            # Estrutura de dados de uma operação (job, release, processing,
│                          #   due date, tool set)
├── Operation.cpp          # Implementação dos construtores de Operation
├── Configuracao.h         # Parâmetros configuráveis da ILS (buscas ativas, limites,
│                          #   percentual de perturbação, tempo máximo)
├── Run.cpp                # Orquestrador: itera sobre uma pasta de instâncias, chama Main
│                          #   para cada arquivo e gera RESUMO.csv ao final
├── Makefile               # Compilação dos executáveis Main e Run
│
└── pmstr-basecases/       # Instâncias de teste
    ├── 2M38/              # 2 máquinas, 38 operações
    │   ├── Scenario1/     # Instâncias por cenário (variação do release time factor R)
    │   ├── Scenario2/
    │   └── Scenario3/
    ├── 2M46/              # 2 máquinas, 46 operações
    ├── 6M140/             # 6 máquinas, 140 operações
    ├── 6M163/             # 6 máquinas, 163 operações
    └── ParameterTuning/   # Instâncias para ajuste de parâmetros
```

### Formato das Instâncias

Cada arquivo `.csv` possui um cabeçalho com os parâmetros da instância seguido das operações:

```
O,<num_operacoes>
M,<num_maquinas>
T,<num_conjuntos_ferramentas>
C,<capacidade_magazine>

Job,Operation,Release time,Processing time,Due date,Tool set,Tool set size
1,1,0,6.33,54.33,6,17
...
```

O **fator R** no nome do arquivo (ex: `2M38_0.24R.csv`) indica a proporção entre release times e due dates — quanto maior, mais apertados os prazos.

Os cenários variam a estrutura dos jobs e operações (ex: quantidade de operações por job, distribuição de ferramentas).

---

## Compilação

Requer **g++** com suporte a C++20.

```bash
# Compilar tudo (Main e Run)
make

# Compilar apenas o solver
make Main

# Compilar apenas o orquestrador
make Run

# Limpar executáveis
make clean
```

Os executáveis são gerados com `-O3 -march=native` para melhor desempenho.

---

## Execução

### Rodar todas as instâncias de uma pasta

```bash
./Run <caminho/para/pasta/de/instancias> <num_repeticoes>
```

**Exemplo:**

```bash
./Run pmstr-basecases/2M38/Scenario1/ 10
```

Isso executa o solver **10 vezes** para cada arquivo `.csv` encontrado na pasta, gravando os resultados individuais em `<pasta>/solucoes/SAIDA_<exec>_<instancia>.csv` e um resumo consolidado em `<pasta>/solucoes/RESUMO.csv`.

> **Instâncias maiores** (`6M140`, `6M163`) podem demandar horas de execução. Recomenda-se usar `nohup`:
>
> ```bash
> nohup ./Run pmstr-basecases/6M140/Scenario1/ 10 > log_6M140.txt 2>&1 &
> ```

### Rodar uma instância individualmente

```bash
./Main <arquivo_saida.csv> < <arquivo_instancia.csv>
```

**Exemplo:**

```bash
./Main saida.csv < pmstr-basecases/2M38/Scenario1/2M38_0.24R.csv
```

---

## Saída

Cada arquivo de saída individual contém:

```
Instance_name,O,M,T,C,Solucao_Inicial,ILS,Tempo de_execucao(s)
2M38,38,2,9,80,<valor_inicial>,<valor_ils>,<tempo_s>
```

O `RESUMO.csv` consolida todos os resultados com os parâmetros de configuração utilizados no cabeçalho.

---

## Configuração dos Parâmetros

Edite `Configuracao.h` e recompile para ajustar o comportamento da ILS:

```cpp
namespace Configuracao {
    int RE_INSERTION  = 1;   // 1 = ativa Re-insertion, 0 = desativa
    int INSERTION_IM  = 1;   // 1 = ativa Insertion-IM, 0 = desativa
    int TWO_SWAP      = 1;   // 1 = ativa Two-Swap, 0 = desativa

    int    LIMITE_ITERACOES_SEM_MELHORIA = 100;   // Critério de parada (iterações consecutivas)
    double PERCENTUAL_PERTURBACAO        = 0.02;  // Fração das operações trocadas na perturbação
    int    LIMITE_TEMPO_HORAS            = 2;     // Tempo máximo em horas
}
```

### Debug

Para habilitar logs detalhados das buscas locais em tempo de execução, defina a variável de ambiente antes de executar:

```bash
PMSTR_DEBUG_LOCAL_SEARCH=1 ./Main saida.csv < instancia.csv
```

---

## Referência

> Artigo de referência do problema:  
> *"[A matheuristic for parallel machine scheduling with tool replacements](https://doi.org/10.1016/j.ejor.2020.09.050)"*
