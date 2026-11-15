# PMSTR — ILS para Escalonamento em Máquinas Paralelas com Troca de Ferramentas

Implementação da metaheurística **Iterated Local Search (ILS)** combinada com **Variable Neighborhood Descent (VND)** e adaptação da política **KTNS (Keep Tool Needed Soonest)** orientada ao tamanho das ferramentas para o problema de Escalonamento em Máquinas Paralelas com Trocas de Ferramentas (*Parallel Machine Scheduling with Tool Replacements* — **PMSTR**), desenvolvida como Trabalho de Conclusão de Curso (TCC).

> **Repositório Pai / Base do Projeto:**  
> - **PMSTR (Repositório Principal):** [https://github.com/anaclpsouza/PMSTR](https://github.com/anaclpsouza/PMSTR)  
> - **Artigo de Referência do Problema e Instâncias:**  
>   Dang, Q.-V., van Diessen, T., Martagan, T. G., & Adan, I. J. B. F. (2021). *"[A matheuristic for parallel machine scheduling with tool replacements](https://doi.org/10.1016/j.ejor.2020.09.050)"*. European Journal of Operational Research, 291(2), 640–660.

---

## 1. O Problema (PMSTR)

O **PMSTR** consiste em alocar e sequenciar um conjunto de $O$ **operações** (pertencentes a diferentes *jobs*, com restrições de precedência entre operações de um mesmo *job*) em $M$ **máquinas paralelas idênticas**, respeitando:

- **Precedência entre operações do mesmo Job:** A operação $O_{j,k}$ de um *job* $j$ só pode iniciar após a conclusão da operação predecessora $O_{j,k-1}$ (mesmo que alocada em outra máquina).
- **Release Time ($r_{j,k}$):** Instante mais cedo em que a operação pode iniciar seu processamento.
- **Processing Time ($p_{j,k}$):** Tempo de processamento da operação.
- **Due Date ($d_{j,k}$):** Prazo de entrega desejado para a conclusão da operação.
- **Restrição de Magazine e Troca de Ferramentas:**
  - Cada operação exige um conjunto de ferramentas específico (`toolSetId` $\in \{1, \dots, T\}$) que ocupa um determinado número de slots (`toolSetSize`) no magazine da máquina.
  - Cada máquina possui capacidade máxima de $C$ slots no magazine.
  - Caso o conjunto de ferramentas exigido pela operação atual não esteja carregado no magazine, realiza-se uma **troca de ferramentas (setup)** com duração $\tau = 1\text{h}$, removendo ferramentas excedentes até que a ocupação do magazine respeite $u_i \le C$.

### Função Objetivo

A função objetivo minimiza a soma ponderada do **atraso total (*Total Tardiness*)** e do **custo total de trocas de ferramentas (*Total Setups*)**:

$$\min \; w_d \sum_{j,k} T_{j,k} + w_s \cdot \tau \cdot \text{Setups}$$

onde $T_{j,k} = \max(0, C_{j,k} - d_{j,k})$ é o atraso da operação e $w_d = w_s = \tau = 1{,}0$.

---

## 2. Avaliação Dupla de KTNS (Original vs. Adaptado por Tamanho)

Em cada chamada da função objetivo (`ObjectiveFunction.cpp`), o escalonamento é avaliado pelas **duas versões do KTNS** e retorna o **menor valor objetivo** encontrado entre elas, contabilizando quantas vezes cada abordagem venceu (`KTNS_Antigo`, `KTNS_Novo`) ou empatou (`Empates`, usando o `KTNS_Novo` como desempate):

- **KTNS Antigo (Original):** Quando o magazine excede a capacidade $C$, remove ferramentas sem uso futuro (`prioridade == -1`) ou, na ausência destas, aquela cujo próximo uso está mais distante no futuro (`maior prioridade`).
- **KTNS Novo (Orientado ao Tamanho):** Como os conjuntos de ferramentas possuem tamanhos variáveis (`toolSetSize`), o custo de remoção é `0` e cada reinserção futura custa `1` setup ($\tau = 1\text{h}$), remover ferramentas pequenas apenas pela distância de reuso pode forçar a remoção de múltiplas ferramentas. O KTNS Novo opera em duas etapas:

1. **Etapa 1 — Ferramentas sem uso futuro na máquina (`prioridade == -1`):**
   - Prioriza sempre remover ferramentas que não serão mais necessárias na máquina (custo futuro de reinserção igual a zero).
   - **Critério de desempate:** Entre todas as ferramentas com `prioridade == -1`, remove primeiro a de **maior tamanho (`setsSize`)**, liberando o máximo de slots possível de uma só vez.
2. **Etapa 2 — Ferramentas com uso futuro (`prioridade > 0`):**
   - Caso todas as ferramentas carregadas ainda sejam necessárias futuramente na máquina, escolhe para remoção a ferramenta de **maior tamanho (`setsSize`)**, reduzindo a quantidade de ferramentas expulsas e, consequentemente, o número de reinserções futuras.
   - **Critério de desempate:** Em caso de empate no tamanho, remove a ferramenta com **maior distância até o próximo uso (`prioridade`)**.

---

## 3. A Metaheurística: Iterated Local Search (ILS)

### Fluxo Geral

```text
Solução Inicial (melhor entre 3 heurísticas construtivas)
      │
      ▼
Intensificação Inicial (Re-insertion → Insertion-IM → Two-Swap)
      │
      ▼
┌───────────────────────────────────────────────────────────────┐
│  Coleta operações em espera de precedência (ops_emEspera)     │
│       │                                                       │
│       ▼                                                       │
│  Perturbação Enviesada (pertubacao_enviesada)                 │
│       │                                                       │
│       ▼                                                       │
│  VND (Re-insertion → Insertion-IM → Two-Swap)                 │ ◄── Repete até atingir o limite
│       │                                                       │     de iterações sem melhoria
│       ▼                                                       │     ou o limite de tempo (2h)
│  Critério de Aceitação (custo ≤ 110% da melhor solução)       │
└───────────────────────────────────────────────────────────────┘
      │
      ▼
Melhor Solução Encontrada
```

### 3.1. Geração da Solução Inicial (`Main.cpp`)
Três heurísticas construtivas são avaliadas e a solução viável de menor custo inicial é selecionada:
1. **`distInicial` (Aleatória por Job):** Embaralha os *jobs* aleatoriamente e aloca todas as operações de cada *job* na máquina de menor carga acumulada (repetindo até obter uma solução factível).
2. **`enviesadaPorRelease`:** Ordena as operações por `releaseTime` crescente (com desempate por `dueDate`, `idJob` e `idOp`) e aloca na máquina de menor carga, mantendo operações do mesmo *job* na mesma máquina.
3. **`enviesadaPorDueDate`:** Ordena as operações por `dueDate` crescente (com desempate por `releaseTime`, `idJob` e `idOp`) e segue a mesma regra de alocação por menor carga.

### 3.2. Perturbação Enviesada (`pertubacao_enviesada`)
Seleciona $\max(1, \lceil O \times \text{PERCENTUAL\_PERTURBACAO} \rceil)$ operações priorizando aquelas que sofreram espera por precedência entre máquinas (`ops_emEspera`) na solução base (completando com operações aleatórias quando necessário). Para cada operação selecionada, sorteia uma máquina de destino distinta e a insere na posição válida que minimiza a função objetivo.

### 3.3. Buscas Locais no VND (`Buscas.cpp`)
Executadas com estratégia **First Improvement** e ordenadas pelos atrasos acumulados por máquina (`tardiness_maq`):

| Busca Local | Escopo | Descrição |
| :--- | :--- | :--- |
| **`re_insertion`** | Intra-máquina | Remove uma operação de uma máquina (iniciando pela mais atrasada) e testa sua reinserção em outra posição da **mesma máquina**. |
| **`insertion_im`** | Inter-máquinas | Remove uma operação da máquina de origem (da mais atrasada para a menos atrasada) e testa sua inserção em posições de **outras máquinas**. |
| **`two_swap`** | Intra-máquina | Troca a posição entre pares de operações dentro da **mesma máquina**, priorizando as máquinas com maior atraso. |

---

## 4. Calibração de Parâmetros por Dimensão (`Configuracao.h`)

Os hiperparâmetros da ILS foram calibrados de maneira independente utilizando o pacote **irace** (*Iterated Racing for Automatic Algorithm Configuration*) para cada porte de máquinas. O sistema seleciona automaticamente a configuração correspondente ao ler o número de máquinas $M$ da instância:

| Parâmetro | **2 Máquinas (`2M38`, `2M46`)** | **6 Máquinas (`6M140`, `6M163`)** |
| :--- | :---: | :---: |
| `RE_INSERTION` | `1` (Ativo) | `0` (Inativo) |
| `INSERTION_IM` | `1` (Ativo) | `1` (Ativo) |
| `TWO_SWAP` | `0` (Inativo) | `1` (Ativo) |
| `LIMITE_ITERACOES_SEM_MELHORIA` | `100` | `300` |
| `PERCENTUAL_PERTURBACAO` | `0.02` (2%) | `0.01` (1%) |
| `LIMITE_TEMPO_HORAS` | `2` horas | `2` horas |

---

## 5. Estrutura do Repositório

```text
PMSTR/
├── Main.cpp               # Leitura da instância, solução inicial, chamada da ILS e escrita dos resultados
├── Buscas.cpp             # Implementação da ILS, perturbação enviesada e buscas locais (VND)
├── Buscas.h               # Cabeçalho das buscas locais e da ILS
├── ObjectiveFunction.cpp  # Simulação do escalonamento, validação de precedência/ciclos e KTNS por tamanho
├── ObjectiveFunction.h    # Declaração da função objetivo
├── Operation.cpp          # Construtores da classe Operation
├── Operation.h            # Definição da classe Operation
├── Configuracao.h         # Parâmetros calibrados por dimensão (2M e 6M)
├── Run.cpp                # Executor em lote com retomada automática e geração de RESUMO.csv e MATRIZES_FINAIS.txt
├── Makefile               # Script de compilação (g++ -O3 -march=native -std=c++20)
├── backup_solucoes.sh     # Utilitário para mover e organizar pastas 'solucoes/' em 'backup-<data>/'
└── pmstr-basecases/       # Conjunto de instâncias de benchmark (Cenários 1, 2 e 3)
    ├── 2M38/              # 2 máquinas, 38 operações
    ├── 2M46/              # 2 máquinas, 46 operações
    ├── 6M140/             # 6 máquinas, 140 operações
    └── 6M163/             # 6 máquinas, 163 operações
```

---

## 6. Compilação e Execução

### Compilar o Projeto
Requer compilador `g++` com suporte a `C++20`:

```bash
# Compila os executáveis Main e Run
make

# Limpa os binários compilados
make clean
```

### Executar uma Bateria de Instâncias (`./Run`)

```bash
./Run <caminho/para/pasta/do/cenario> <numero_de_repeticoes>
```

**Exemplo:**
```bash
./Run pmstr-basecases/2M38/Scenario1/ 5
```

- **Retomada automática:** Caso a máquina seja desligada ou a execução seja interrompida, basta rodar o mesmo comando `./Run` novamente. Ele verifica os arquivos `SAIDA_*` já concluídos (`size > 0`), pula as execuções finalizadas e reexecuta automaticamente eventuais arquivos vazios ou faltantes.
- **Saídas geradas em `<cenario>/solucoes/`:**
  - `SAIDA_<exec>_<instancia>.csv`: resultado individual da execução com as colunas `Cenario,Execucao,Instancia,O,M,T,C,Solucao_Inicial,ILS,Tempo_Execucao(s),KTNS_Antigo,KTNS_Novo,Empates`.
  - `RESUMO.csv`: planilha consolidada com os parâmetros utilizados e os resultados de todas as execuções do cenário.
  - `MATRIZES_FINAIS.txt`: sequenciamento final das operações em cada máquina (`M1..Mm`) para cada execução.

### Fazer Backup antes de uma Nova Rodada

Para arquivar todos os resultados atuais em uma pasta organizada `backup-YYYY-MM-DD_HH-MM-SS/` e limpar as pastas `solucoes/` originais:

```bash
./backup_solucoes.sh
```

### Executar uma Única Instância Diretamente (`./Main`)

```bash
./Main saida.csv < pmstr-basecases/2M38/Scenario1/2M38_0.24R.csv
```
