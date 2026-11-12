# Experiências investigativas no NOESE

A função desta entrega é permitir recuperar e criticar o percurso que produziu
uma contribuição, inclusive quando termina em falha. A interface fica em
`/experiencias`, ligada à navegação da página inicial e da mesa. É possível
preservar uma captura, consultar o foco e os eventos, exportar os originais e
registrar uma avaliação com pertinência, alcance e atenção separados.

Uma experiência continua sendo uma **investigação**, com fonte original,
âncora, nota candidata, versões e avaliações. Não foi criado outro banco ou uma
nova classe de memória. A associação de uma captura a um percurso existente
cria uma nova versão desse percurso e conserva a anterior.

## Dois tipos de captura

- `trace`: JSON Lines do executor, com missão, eventos e desfecho. A importação
  verifica sequência dos IDs, hashes das mensagens MCP, correspondência de
  requisições e respostas, referências anteriores ao foco e ausência de troca
  retrospectiva do foco na síntese. Requisições sem resposta ficam contadas.
- `secondary_report`: texto disponível de uma investigação passada. O original
  é preservado literalmente, mas as respostas brutas e o momento inicial da
  escolha ficam **não registrados**. Não são reconstruídos a partir da narrativa.

Integridade significa correspondência dos bytes preservados e coerência da
ordem registrada. Não autentica o produtor, não prova completude em relação a
uma conversa externa e não certifica pertinência, alcance ou relevância.
Produção e finalidade são declaradas pelo importador. Um relatório secundário
não recebe automaticamente a classificação de captura instrumental.

A memória anterior tem política explícita: `none`, `archive` ou `not_recorded`.
Uma captura que declara exclusão da memória NOESE é recusada se contiver
consultas ao acervo. O harness também bloqueia essas consultas antes do despacho
quando executado com `--memory-policy none`; somente `capabilities` do NOESE
permanece disponível nessa condição. O MÉTIS e seu catálogo continuam fontes
acessíveis; isso não permite afirmar descoberta independente de seu conteúdo.

## MCP e HTTP

`preserve_experience` exige escrita autorizada e `request_id` estável. Recebe
`run_id`, `title`, `capture`, `producer`, `purpose`, `memory_policy`, `limitations`
e **um** de `journal` ou `report`. `inquiry_id` permite associar a captura a um
percurso existente sem substituir seus registros anteriores. A captura exige
até 650.000 bytes e 200 eventos; o relato tem também o limite textual do schema.
O transporte MCP mantém seu teto de 1 MiB para a mensagem serializada.

A operação confirma fonte, âncora e associação na mesma transação, usando a
idempotência existente. Metadados reservados de experiência não podem ser
fornecidos pela operação genérica de criação ou edição de registros.

`experiences` lista os percursos com captura na janela autorizada, com paginação
sobre as investigações examinadas. `experience(id)` recupera metadados, foco,
exposição à memória, desfecho e avaliações. No MCP, `include_events=false` é o
padrão para não devolver o contexto inteiro em cada recuperação. Com `true`,
retorna os eventos; os bytes originais também podem ser lidos por `artifact` em
páginas. A interface HTTP usa `GET /api/v1/experiences` e
`GET /api/v1/experiences/<id>`, e importa por `POST /api/v1/experiences` com token
de sessão e UUID. A avaliação usa os registros de avaliação existentes, com
alvo na investigação e dimensões separadas, sem homologação automática.

O executor preserva sua versão exata no diretório privado antes de inferir.
Depois de importar os eventos e conferir a exportação, associa também o journal
à experiência recuperável. Falhas antigas continuam preservadas. A exclusão
sensível e a exportação seguem as dependências das entidades existentes.

## Resultado da correção operacional em 2026-11-12

O ensaio original `41ea4bf3-0f8c-4e66-bb7d-05b43005e9ba` permanece uma falha
HTTP anterior à geração, agora consultável como experiência com 22 eventos.

Às **14h01, America/Sao_Paulo**, a execução
`034badca-6498-469a-9d83-c2173141c5a7` demonstrou que o provedor já compilava a
gramática e gerava resposta: 283 tokens. O contrato rejeitou uma decisão vazia
antes de executar as ferramentas propostas. Essa saída e seus 22 eventos foram
preservados. A observação mudou a correção: omitir apenas `maxLength` da
gramática, conservando `minLength: 1`; o contrato integral continua validado
localmente. Aprendizado candidato: a adaptação da gramática não deve remover
uma restrição suportada e necessária. Escopo: este provedor e este contrato;
promoção: **candidate**. Nenhum resultado científico motivou a alteração.

Às **14h07**, a execução `081e27d7-30b4-4115-9bb4-1440217ad843` usou o mesmo
modelo `qwen2.5:14b`, pergunta aberta, orçamento e parâmetros, com memória NOESE
excluída. Durou aproximadamente 59 segundos, gerou 2.075 tokens e contabilizou
oito consultas reais, incluindo as duas verificações iniciais. O modelo escolheu
um foco e consultou catálogo, dossiê e confronto intramunicipal para Pelotas.
Os 50 eventos e respostas foram preservados e recuperados com hash conferido.

O desfecho é **`CANDIDATE_REJECTED`**: não houve consulta declarada de alternativa.
A saída reiterou uma hipótese já disponível no catálogo e incluiu referências
de citação que ainda exigiriam verificação; não se demonstra novidade,
confrontação adequada ou sustentação científica. Não houve nova execução para
tentar obter uma contribuição aceita. A integração operacional percorreu o
laço, mas a função investigativa pretendida continua parcialmente não demonstrada.

O texto BB-001 fornecido pelo usuário foi preservado como relatório secundário,
com memória consultada declarada e ausência das respostas originais explícita.
Não se refizeram suas consultas para fabricar a evidência histórica ausente.
As questões científicas sobre Bagé, comparabilidade dos censos e classificação
de achados no MÉTIS permanecem abertas; nenhum dado do MÉTIS foi alterado.

## Verificação

`scripts/verify.sh` inclui sete cenários de experiência pelo MCP real, além dos
grupos existentes. `tests/bb_harness.py` inclui bloqueio de memória e persistência
recuperável. `tests/experience-browser.cjs` usa acervo temporário e verifica
importação dos dois tipos, texto não confiável, recuperação, avaliação separada,
exportação literal, isolamento J0/J2 e navegação responsiva. Os registros de
avaliação desse teste são sintéticos e não entram no acervo de uso.
