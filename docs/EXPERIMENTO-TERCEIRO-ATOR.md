# Experimento: participação investigativa do NOESE

Preparado em 2026-11-11 (America/Sao_Paulo). Pergunta: até onde a implementação existente consegue participar de uma investigação sem devolver ao pesquisador a reconstrução inteira? Nenhuma transformação específica é critério obrigatório de sucesso.

## Inspeção inicial

Na inspeção local, `var/noese.sqlite` contém zero fontes e zero execuções. Os arquivos privados de `fixtures/lab001/` estão ausentes. O provedor local responde e informa `qwen2.5:3b` instalado. **O ensaio investigativo não foi executado por falta do corpus autorizado.** Isso é uma condição de entrada ausente, não um resultado negativo do observador. Nenhuma descoberta foi produzida ou simulada.

A documentação do LAB-001 descreve 21 excertos parciais, sem datas individuais. Sua eventual reposição permite um ensaio de desenvolvimento; não constitui corpus novo nem validação independente. Este protocolo não transforma ordem editorial em cronologia histórica.

Atualização após o compartilhamento do ZIP pelo usuário: o corpus foi reposto e o primeiro ensaio foi concluído. Consulte o [resultado preservado](EXPERIMENTO-TERCEIRO-ATOR-RESULTADO.md); o diagnóstico de ausência acima descreve a inspeção anterior à importação.

## Entrada e separação da avaliação

1. Identificar o corpus autorizado e seu escopo de uso local. Preservar os originais, hashes, sequência das falas, autoria atribuída, datas conhecidas e contextos disponíveis. Marcar lacunas e metadados desconhecidos; não completá-los por inferência. Conferir o que o importador efetivamente preservou antes de analisar.
2. Usar um acervo experimental separado. Importar as conversas como fontes, sem anexar rubricas, resultados esperados, este protocolo ou a interpretação original. Caso a interpretação já esteja nas próprias conversas, registrar essa exposição: o ensaio poderá medir reconstrução, mas não descoberta sem conhecimento prévio.
3. Preservar a interpretação original em arquivo separado, com hash anterior ao ensaio, sem fornecê-la ao observador. Não exigir que o pesquisador reconstrua outra vez a trajetória antes da execução. Se essa interpretação ainda não existir em registro, declarar que o confronto posterior não é uma referência previamente fixada.

## Execução com o que existe

Para o LAB-001, após repor o ZIP autorizado com `scripts/import_lab.py`:

```bash
python3 scripts/semantic-smoke.py --check
python3 scripts/semantic-smoke.py --model qwen2.5:3b --timeout 1200
```

`--check` só verifica presença dos arquivos, executável e modelo local; não inicia inferência nem certifica integridade ou autorização. O importador e o serviço conferem o pacote. O script executa o observador em acervo separado sob `var/experiments/`, verifica as citações aceitas contra os bytes originais e preserva `run.json` e `summary.json` quando há uma execução concluída. Timeout ou falha não produzem esses relatórios de sucesso; preservar também o log e a causa de interrupção. O script é específico do LAB-001 e não importa conversas arbitrárias.

Para outro corpus, iniciar `./scripts/run.sh --data var/experimento-terceiro-ator --port 8788`, importar as fontes pela mesa e habilitar o observador nas configurações. Não formular uma pergunta que contenha a conclusão desejada. Antes de interpretar o resultado, conferir a cobertura preservada na execução. Não tratar uma única transcrição longa como inteiramente lida: o observador usa até 32 fontes, 4.000 bytes por fonte e 24.000 bytes totais. A importação e a ordenação de exportações arbitrárias ainda exigem conferência; esse trabalho conta como custo do experimento.

Preservar a primeira execução antes de qualquer crítica. Examinar a interpretação, suas evidências, alternativas, contraponto e propostas de contribuição. Voltar aos originais para conferir sequência e contexto. Uma citação literal correta não demonstra transformação, causalidade ou autoria autêntica. O contraponto é outro passe do mesmo modelo, sem independência.

Só então abrir a interpretação original e registrar convergências, divergências e omissões. Uma alternativa enunciada pelo modelo não equivale a uma alternativa investigada: registrar se houve confronto efetivo com evidências, ou apenas sugestão. Contribuição proposta deve manter separados pertinência, capacidade disponível e autorização. Não executar propostas externas como consequência automática do ensaio.

## Registro de avaliação

Preencher com referência à execução e aos originais, aceitando “não demonstrado” e “inconclusivo”. O avaliador julga a saída preservada; não escreve uma resposta melhor para atribuí-la ao sistema.

| Aspecto | Evidência a registrar |
|---|---|
| Reconstrução | O que mudou, entre quais falas, com qual apoio; lacunas de sequência/autoria/contexto |
| Interpretações concorrentes | Alternativas consideradas, apoio e contraevidência efetivamente examinados; alternativas apenas mencionadas |
| Terceiro ator | Insuficiência identificada, contribuição proposta, procedimento, capacidade real e autorização necessária |
| Confronto | Convergências e divergências com a interpretação original; novidade versus recuperação de conclusão já presente |
| Autonomia e custo | Tempo até a saída, intervenções e minutos humanos para importar, localizar provas, corrigir e julgar; tarefas concluídas pelo sistema e tarefas devolvidas |
| Consequência | O que a contribuição efetivamente alterou; se não executada, registrar apenas potencial |
| Revisão | Insuficiência observada, evidência correspondente e menor mudança justificável para um próximo ensaio |

Não somar sugestões como descobertas. Não atribuir ganho de tempo sem uma comparação observada. Avaliar separadamente execução técnica, sustentação interpretativa e utilidade investigativa. Repetições após críticas devem permanecer identificadas como novos ciclos; não substituir silenciosamente a primeira saída. Resultado vazio, rejeitado, interrompido ou inconclusivo deve permanecer no registro.

## Escolha dos instrumentos

O observador local é a primeira tentativa porque já relaciona textos e oferece interpretação com alternativas. SQLite, originais e âncoras servem à rastreabilidade; não são por si uma investigação semântica. Para um corpus pequeno que caiba no contexto, leitura direta evita acrescentar infraestrutura sem necessidade demonstrada.

Se a cobertura excluir material relevante, recuperação contextual pode ser o próximo instrumento, desde que preserve o contexto das falas e exponha o material não recuperado. RAG e vetores exigem demonstrar melhoria de cobertura ou recuperação; grafos exigem uma questão relacional que justifique sua manutenção. JEV não é requisito deste ensaio: qualquer uso deve explicitar a operação investigativa, sua capacidade disponível e a evidência que permite produzir. A escolha será revista a partir de falhas observadas.

Limites atuais: o observador não busca documentos externos, não executa as contribuições que propõe e não realiza investigação iterativa autônoma para resolver alternativas. Seu exame limitado e dois passes sem ferramentas permitem avaliar uma participação candidata; não demonstram todo o propósito de terceiro ator.
