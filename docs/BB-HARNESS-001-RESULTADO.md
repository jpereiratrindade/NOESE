# BB-Harness v0.1 — primeiro resultado

Executado em **2026-11-12, 10:45, America/Sao_Paulo**. Resultado:
**`OPERATIONAL_FAILURE`**. A investigação completa ainda não foi demonstrada.
Não houve foco escolhido, contribuição, confronto nem abstenção investigativa.
Não foi feita outra execução para substituir este resultado.

## Evidência observada

O NOESE e o MÉTIS responderam à negociação MCP, ao catálogo e às consultas reais
de `capabilities`. O Ollama 0.32.5 recebeu a primeira requisição com
`qwen2.5:14b`, digest
`7cdf5a0187d5c58cc5d369b255592f7841d1c4696d45a8c8a9489440385b22f6`.
Retornou HTTP 400 antes de gerar o foco. O log do serviço registra:

```text
number of repetitions exceeds sane defaults, please reduce the number of repetitions
Failed to initialize samplers: failed to parse grammar
```

A gramática gerada continha repetições `char{1,4000}` derivadas dos limites de
comprimento dos textos no schema. Essa é evidência de falha operacional da
integração com o compilador de gramática, anterior à geração investigativa.
O contador local não registrou tokens gerados; não é medição de desempenho
investigativo nem evidência de incapacidade do modelo para a tarefa.

O primeiro executor registrou apenas o tipo `HTTPError`, sem guardar o corpo da
resposta HTTP. O diagnóstico depende da evidência suplementar do journal do
serviço. Essa lacuna de observabilidade não foi ocultada nem preenchida com uma
resposta reconstruída.

## Recuperação no NOESE

- Execução: `41ea4bf3-0f8c-4e66-bb7d-05b43005e9ba`.
- Percurso: `b402684506946eaa0f9641b7cb3500cc`, J2.
- Título na mesa: `BB-Harness v0.1 · 41ea4bf3-0f8`.
- 22 eventos importados como fontes, ancorados e vinculados a notas candidatas.
- Exportação via MCP recuperada; bytes das 22 fontes conferidos por SHA-256.
- Journal original: SHA-256 `ba89e3656a0356f95e2d7b62b2766d96e86afa68267e7206f930341aabe309de`.

Evidência privada em `var/experiments/bb-v01-first/`: `events.jsonl`,
`result.json`, `persistence.json`, recibos de transporte e
`provider-journal.log`. O código exato usado no ensaio foi reconstruído e seu
hash conferido contra o registrado **antes** da execução; está em
`executor-at-run.py` (SHA-256
`da00ec9ecc4f29c7625b9c1f8a5bad71192bc7e5149f8256c007357ccd844b07`).
O journal original permanece inalterado. O diagnóstico posterior é suplementar.

## Correção delimitada e verificação

Expectativa anterior: o schema estrutural aceito pelo cliente seria compilável
pelo provedor. A recusa da gramática mudou essa expectativa e impediu o início
da investigação. O adaptador passou a retirar limites de comprimento **apenas
da gramática enviada ao Ollama**; o contrato completo continua no prompt e na
validação local. Modelo, questão, orçamento e critérios não foram ajustados com
base em achados científicos, pois não houve geração.

Também passou a preservar status e corpo limitado de erros HTTP. Os testes
verificam essa captura, ausência de retry e manutenção dos limites locais mesmo
quando omitidos da gramática. A correção ainda não foi validada por uma nova
inferência real; não se declara o gate de integração satisfeito.

Aprendizado candidato: compatibilidade de schema precisa ser verificada também
no compilador de geração do provedor. Uma validação JSON correta no executor
não demonstra essa compatibilidade. Escopo: este ensaio, Ollama 0.32.5 e este
schema. Promoção: **candidate**. A fronteira que falhou foi a verificação de
integração; a insuficiência de registro do erro foi uma falha de observabilidade.

Verificação técnica final: 15 testes específicos do harness, com provedor
determinístico e persistência no MCP real do NOESE; suíte existente
`scripts/verify.sh` aprovada nos três grupos CTest. Nenhum desses testes comprova
perspicácia ou relevância científica.

## O que permanece pendente

A recuperação de um percurso de falha foi demonstrada. Seleção autônoma de foco,
consultas escolhidas pelo modelo, confronto e contribuição relevante permanecem
**não demonstrados**. Pertinência, alcance e atenção merecida não podem ser
avaliados nesta saída, pois não houve contribuição. A avaliação humana do
experimento e de suas limitações permanece pendente; o estado M1 do NOESE não
foi promovido.
