# Primeiro ensaio com o corpus autorizado

Executado em 2026-11-11, America/Sao_Paulo. **Resultado: nenhuma interpretação aceita; capacidade investigativa pretendida não demonstrada neste ensaio.** Não se conclui que o corpus não contenha transformações relevantes.

## Condições e evidência

O ZIP indicado pelo usuário foi preparado por `scripts/import_lab.py`, com hash do corpus conferido contra o manifesto. Os documentos anexos foram tratados como referência, sem executar suas instruções. Somente os 21 excertos de conversa foram importados para a base experimental. O documento de contraste ficou fora da entrada e foi consultado após a saída estar preservada.

Execução `f9a45cde04dec271110cbe5ea7fc501f`, motor `semantic-confrontation-v1`, modelo local `qwen2.5:3b`, J2, temperatura 0, seed 7, CPU, sem ferramentas ou consultas externas. Tempo observado pelo executor: 147,2 segundos. Corpus `PARTIAL_EXCERPTS`, SHA-256 `e7fb9bc53c311de72bb205fb36ecb7d92924223d49fad0da3bc88fc519c90901`.

Cobertura efetiva: 21 de 21 fontes, 2.549 bytes, sem exclusões ou truncamentos registrados. Conferência posterior comparou os textos, hashes, autores atribuídos, ordem editorial e datas nulas com o corpus autorizado. Esses metadados não autenticam autoria nem fornecem cronologia histórica. A conversa já contém formulações interpretativas: reconstruí-las não demonstraria descoberta de algo antes não percebido.

Evidências privadas em `var/experiments/semantic-188bc513329f/`: `run.json` (entrada, geração bruta, rejeição e resultado), `summary.json`, `audit.json`, `server.log` e o acervo em `archive/`. A base de uso `var/noese.sqlite` não recebeu as fontes experimentais. Documentos, corpus e saídas privadas permanecem fora do Git.

## O que o sistema realizou

O modelo gerou um candidato genérico sobre a importância da análise histórica, apoiado em uma única citação. A citação corresponde literalmente a uma fonte, conforme auditoria posterior. O validador rejeitou o candidato porque a interpretação exige de dois a oito trechos de apoio. Nenhum achado, oportunidade ou contribuição desse candidato foi publicado. O contraponto foi dispensado pelo fluxo porque não havia candidato validado.

O status preservado é `NO_GROUNDED_OPPORTUNITY`. Neste caso, significa **saída gerada sem sustentação mínima exigida**, não uma investigação bem-sucedida que demonstrou ausência de oportunidades.

| Dimensão | Observação nesta execução |
|---|---|
| Transformação intelectual | Não reconstruiu uma passagem específica entre posições com evidência de origem e mudança; reformulou uma afirmação já presente |
| Evidências | Uma citação literal no rascunho, insuficiente para o contrato de validação; nenhum apoio aceito |
| Alternativas | Listou frases do corpus sem desenvolver e confrontar interpretações concorrentes |
| Terceiro ator | Propôs genericamente ampliar a análise; não delimitou uma contribuição operacional sustentada |
| Capacidade e autorização | Associou a proposta a uma capacidade de regras de cobertura, inadequada ao alcance semântico proposto; o campo de autorização trouxe um requisito de projeto, sem delimitar permissão |
| Crítica | Passe de contraponto não executado; não houve investigação adicional de evidências |
| Consequência | Nenhuma contribuição executada e nenhum ganho investigativo demonstrado |

## Confronto posterior e trabalho realizado

O documento original é uma auditoria de coerência de uma diretriz, não uma resposta de referência independente nem validação de software. Ele relaciona mudanças de concepção, crítica aos instrumentos, diferenciação e reflexividade com grupos de excertos. A saída desta execução não reconstruiu essas relações nem ofereceu outra transformação específica sustentada. A convergência temática genérica com o objeto histórico é insuficiente para demonstrar participação investigativa.

Este confronto é análise do agente de desenvolvimento sobre a evidência preservada. Não é produção do NOESE, avaliação humana de aceitação ou validação científica independente. Não se atribui ao sistema a reconstrução feita nesta análise.

O sistema realizou importação, preparação de contexto, inferência, rejeição e preservação sem exigir pergunta ou anotações do pesquisador. A preparação do ensaio e a inspeção das insuficiências foram feitas pelo agente de desenvolvimento. A reconstrução pretendida continuaria por fazer; portanto, não há redução demonstrada desse trabalho. Não foram medidos minutos humanos nem comparação com execução manual.

## Insuficiências para a próxima revisão

1. O contrato pede múltiplos apoios, mas aceita gerar uma lista com um único elemento no schema. Alinhar o contrato estrutural e permitir uma reparação limitada que busque apoio no mesmo corpus, sem incluir nomes, trajetórias ou respostas esperadas. Preservar a primeira rejeição e também admitir falha final.
2. Uma lista textual não demonstra alternativas investigadas. Examinar se um ciclo delimitado de recuperação e confronto melhora a sustentação; registrar buscas, contexto, contraevidência e custo.
3. Propostas precisam de validação de adequação à capacidade e escopo de autorização. O catálogo evita inventar disponibilidade, mas não verifica sozinho que a operação proposta cabe na capacidade escolhida.
4. O relatório deve distinguir candidato rejeitado de exame inconclusivo sem candidato. O executor marcava `source_quotes_verified=true` mesmo com zero citações aceitas verificadas. A primeira evidência foi mantida, a ressalva registrada em `audit.json`, e o script passou a informar contagem e `null` quando não houve citação aceita para conferir.

Nenhuma dessas revisões do motor foi aplicada para tentar obter um resultado favorável nesta execução. Não se repetiu a inferência até aparecer um achado. Uma única execução com um modelo não estabelece o limite geral do NOESE. O resultado justifica investigar geração, validação e ciclos de confronto; não há evidência aqui que exija vetores, grafos ou mudança de arquitetura.

**Aceitação M1 permanece `NOT_READY`.** O ensaio agora fornece uma falha observada e rastreável para orientar a revisão.
