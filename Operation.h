#ifndef OPERATION_H
#define OPERATION_H

class Operation {
public:
    int id;                 // Índice sequencial da operação na instância
    int idJob;              // Identificador do job ao qual a operação pertence
    int idOp;               // Índice da operação dentro do job
    int toolSetId;          // Identificador do conjunto de ferramentas (base 0)
    int toolSetSize;        // Tamanho do conjunto de ferramentas
    double processingTime;  // Tempo de processamento
    double dueDate;         // Data de entrega
    double releaseTime;     // Data de liberação

    Operation();
    Operation(int id,
              int idJob,
              int idOp,
              int toolSetId,
              int toolSetSize,
              double processingTime,
              double dueDate,
              double releaseTime);
};

#endif // OPERATION_H
