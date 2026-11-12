# BB-Harness v0.1 — primeiro experimento vertical

Autorização: plano experimental aprovado pelo usuário em 2026-11-12, com
proveniência da atenção, percurso inteiro recuperável, três avaliações distintas
e julgamento de relevância pelo pesquisador. Não autoriza A/B nesta entrega.

Pergunta: o que um LLM local, com os instrumentos disponíveis, consegue perceber,
investigar e oferecer que não estava determinado pelo gatilho inicial?

Gatilho fixo: **Examine o MÉTIS e identifique uma questão que mereça investigação.**
Não há município, achado esperado ou conclusão de referência no prompt.

## Missão e limites

O executor Python externo usa o MCP stdio já existente nos dois projetos.
NOESE continua sendo o acervo; MÉTIS continua sendo o laboratório independente.
O observador C++ existente não é alterado, assim como sua configuração CPU e
ausência de ferramentas. Não há dependência nova no executável C++.

Uma execução começa por verificar as capacidades de ambos os servidores. O
modelo escolhe o foco e registra pergunta, razão e IDs das respostas que o
motivaram **antes** das consultas seguintes. Depois escolhe ferramentas e
argumentos por saída JSON estruturada do Ollama; o executor valida o schema da
ação e o schema MCP descoberto antes de executar. Essa é seleção instrumental
pelo modelo, com despacho pelo executor; não usa o parser nativo de tool_calls.

A escolha inicial não é substituída por justificativas posteriores. Cada
consulta registra finalidade e razão antes da chamada. A saída deve confrontar
ao menos uma alternativa vinculada a uma consulta de confronto. A etiqueta
`alternative` e a referência existente são verificáveis; a qualidade real desse
confronto exige julgamento humano.

Configuração fixada antes do primeiro ensaio:

| Item | Valor |
|---|---|
| Modelo | `qwen2.5:14b`, já instalado; identidade/digest preservados |
| Temperatura / seed | 0 / 7; não garantem determinismo do provedor |
| Contexto solicitado | 32.768 tokens |
| Inferências | Até quatro escolhas instrumentais e uma síntese |
| Consultas | Até dez, incluindo duas verificações iniciais; duas por rodada |
| Tempo | 1.200 s total; 240 s por inferência; 30 s por operação MCP |
| Geração | Até 1.600 tokens por inferência; 8.000 no total |
| Resposta MCP | Até 30.000 bytes por mensagem; 60.000 acumulados de resultados |
| Entrada Ollama | Até 60.000 bytes serializados; sem truncamento pelo executor |
| Janela NOESE | J2; memória existente acessível pelas ferramentas autorizadas |
| Aceleração | Seleção do Ollama; nenhuma alegação prévia de uso de GPU |

O limite de bytes não é uma contagem exata de tokens. As contagens e tempos
retornados pelo provedor são preservados. O executor não certifica que o
provedor manteve todo o contexto; isso é uma limitação da avaliação. Timeout
interrompe a espera do cliente, podendo a inferência já enviada continuar no
Ollama. Não há repetição automática de inferência ou reparação do prompt.

A lista de leitura é fixa no executor e intersectada com o catálogo real,
exigindo também `readOnlyHint=true`. Anotações isoladas não concedem autoridade.
Argumentos não escolhem comandos, endpoints, transportes ou arquivos locais.
Conteúdo recuperado não amplia a lista. O programa inicia somente os processos
MCP locais configurados pelo operador, sem shell para argumentos do modelo.
Não se trata de isolamento de sistema operacional contra um servidor malicioso.

São proibidos: escrita investigativa pelo modelo, ações externas, mudanças no
MÉTIS, revisão de enunciados existentes, homologação, downloads de modelos,
autonomia contínua, novos bancos, RAG vetorial, grafos e múltiplos agentes.

## Registro e recuperação

`events.jsonl` é um spool privado de execução, sem busca ou recuperação para
investigações futuras. Preserva missão, prompt/configuração, identidade do modelo,
requisições, respostas, foco, decisões, chamadas e desfecho. O pensamento privado
não é solicitado nem armazenado. Respostas MCP aceitas pelo transporte mantêm
os bytes originais UTF-8 e SHA-256; exceder o teto encerra a leitura, sem fingir
que a resposta completa foi preservada. O spool permanece como evidência local.

Uma etapa determinística, separada do modelo, usa outro cliente MCP com escrita
habilitada para abrir o percurso, importar cada evento como fonte, criar âncora
dos bytes e propor nota vinculada. Eventos grandes são divididos em partes UTF-8
ordenadas de até 80.000 bytes, sem descartar dados. A operação usa UUIDs estáveis
derivados da identidade da execução, permitindo repetir **a persistência** após
falha sem repetir inferência ou criar registros duplicados.

Nenhum acesso direto ao SQLite é feito pelo executor. A recuperação é conferida
com `export_inquiry`, comparando os bytes exportados e hashes de todas as fontes
do percurso com os eventos originais. Falha nessa etapa fica como
`PERSISTENCE_FAILED`, separada do resultado investigativo.

As citações finais apontam para blocos textuais das respostas MCP observadas.
O executor verifica ocorrência literal única, offsets UTF-8 e hash do fragmento.
Essa verificação comprova integridade em relação à **resposta instrumental**,
não autentica a fonte científica subjacente. Respostas, afirmações e avaliações
permanecem distinguíveis e sujeitas à crítica.

## Avaliação previamente estabelecida

| Dimensão | Evidência / autoridade |
|---|---|
| Gatilho aberto | Prompt preservado, sem conclusão fornecida |
| Proveniência da atenção | Foco inicial e referências anteriores às consultas |
| Uso instrumental | Chamadas e respostas reais aos dois MCPs |
| Confrontação | Consulta de apoio e de alternativa; mérito avaliado por pessoa |
| Integridade | Referências/citações em respostas, hashes e exportação recuperada |
| Pertinência | Pesquisador avalia se cada evidência sustenta a afirmação |
| Alcance | Pesquisador avalia extrapolação e limitações |
| Atenção merecida | Relação relevante, inconsistência não trivial, insuficiência metodológica ou oportunidade real |
| Trabalho de avaliação | Registrar esforço e dificuldades do pesquisador; não inferir economia |

O pesquisador examina a nota de desfecho e os originais no percurso da mesa
NOESE (`/mesa`), consulta a escolha inicial e o confronto, e registra sua
avaliação vinculada pela interface existente. Até lá: `PENDING_HUMAN`.
Nenhuma ferramenta fabrica essa avaliação. JSON válido e citação correta podem
acompanhar uma contribuição irrelevante.

Desfechos preservados: `CANDIDATE`, `ABSTENTION`, `CANDIDATE_REJECTED`,
`PROTOCOL_REJECTED`, `BUDGET_EXHAUSTED` e `OPERATIONAL_FAILURE`. Abstenção exige
percurso com consultas de apoio e alternativa; erro de ferramenta não a produz.
Saída inválida antes da síntese é rejeição de protocolo, não candidato científico
rejeitado. Uma chamada corretamente rotulada não certifica investigação adequada.

Conclui-se esta missão com executor verificado, primeiro ensaio preservado e
resultado inspecionável. Se o ensaio falhar, relatar o gate não satisfeito;
nenhuma repetição visando uma saída favorável. Perspicácia, aprendizagem e
aceitação científica permanecem não demonstradas até evidência apropriada.
A/B isolado e C (retomada com evidência nova/contraditória) são etapas posteriores.

## Executar

Python 3 e `jsonschema` conforme `config/bb-harness-requirements.txt`; os processos
MCP conservam seus próprios requisitos. Compilar o NOESE antes de executar.

```bash
python3 scripts/bb_harness.py check
python3 tests/bb_harness.py
python3 scripts/bb_harness.py run
# Em caso de falha de persistência, usar o caminho retornado pela execução:
python3 scripts/bb_harness.py persist --output var/experiments/bb-ID
```

`--data`, `--metis`, `--window` e `--model` permitem explicitar os recursos da
missão. `--output` deve ser um diretório novo em `run`/`check`. Modelo, prompt,
limites e configuração ficam registrados antes da primeira inferência.
`check` não infere nem persiste no NOESE; pode inicializar schema pelo MCP.
`run` faz uma única investigação e tenta preservá-la, inclusive quando falha.

Referências técnicas consultadas: [API de chat Ollama](https://docs.ollama.com/api/chat)
e [ferramentas MCP 2025-11-25](https://modelcontextprotocol.io/specification/2025-11-25/server/tools).

O [primeiro resultado](BB-HARNESS-001-RESULTADO.md) preserva uma falha operacional
anterior à escolha do foco. Após esse ensaio, o adaptador passou a omitir
`minLength`/`maxLength` na gramática do provedor para evitar repetições recusadas
pelo compilador. O contrato integral continua no prompt e na validação local.
Essa correção não foi usada para substituir a primeira execução.

## Correção observada durante construção

Expectativa: fontes importadas poderiam fundamentar diretamente as notas do
percurso. Evidência: o teste real contra o NOESE retornou `UNRESOLVED_ANCHOR`;
enunciados exigem âncoras. Mudança: criar âncora dos bytes de cada parte antes da
nota, mantendo os contratos existentes. Isso afeta a recuperabilidade, não o
mérito da investigação. Aprendizado candidato: confirmar suporte no contrato de
domínio, além do schema MCP. Escopo: este adaptador; promoção: **candidate**.
