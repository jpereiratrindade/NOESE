# MCP — acervo compartilhado com a interface

A abordagem segue o CONTINUUM local: processo stdio, leitura por padrão, escrita habilitada explicitamente, operações de domínio compartilhadas com o navegador e origem do cliente registrada. A implementação é própria, em C++26, sem importar código, banco ou núcleo do CONTINUUM.

## Iniciar e conectar

```bash
# Somente ferramentas de leitura; sem abrir porta HTTP:
./scripts/mcp.sh --data /caminho/NOESE/var

# Acrescentar ferramentas de escrita para um cliente confiável:
./scripts/mcp.sh --data /caminho/NOESE/var --allow-writes

# Equivalente, após compilar:
./build/noese mcp --data /caminho/NOESE/var --allow-writes
```

O cliente inicia esse processo e envia mensagens JSON-RPC; não é um console interativo de comandos humanos. `stdout` contém somente respostas MCP; diagnósticos vão a `stderr`. O wrapper também redireciona a compilação inicial para stderr. Fechar stdin encerra o processo. Nenhum servidor web ou observador é iniciado nesse modo.

[config/mcp.example.json](../config/mcp.example.json) contém uma configuração para clientes que aceitam `mcpServers`. Substitua `/CAMINHO/NOESE` pelo caminho absoluto do checkout. O script faz `cd` para o repositório, sem exigir suporte a `cwd` pelo cliente. Para habilitar escrita, acrescente `"--allow-writes"` ao array `args`. A configuração de exemplo é somente um arquivo de referência: não instala ou conecta o NOESE automaticamente ao ChatGPT ou a outro aplicativo.

Use o mesmo `--data` do processo HTTP para compartilhar o acervo. Ambos os processos cooperam por um lock de arquivo e SQLite WAL: leitura, validação de referências, transações e objetos são serializados por operação. As versões esperadas continuam necessárias; um conflito entre navegador e MCP não sobrescreve texto silenciosamente. Ao atualizar de uma versão anterior ao MCP, reinicie também o servidor web para que ambos usem o lock novo.

## Contratos e ferramentas

Implementa o subconjunto de ferramentas da revisão **2025-11-25** do MCP via **stdio** e **Streamable HTTP** (`/mcp`, negociando também `2025-06-18` e `2025-03-26`): `initialize`, `notifications/initialized`, `ping`, `tools/list` e `tools/call`. No modo stdio a versão negociada é sempre `2025-11-25`. Não implementa SSE, resources, prompts, subscriptions ou tasks.

No servidor HTTP (`--data ... --port 8787`), o endpoint `/mcp` fica em somente leitura por padrão e restrito a `127.0.0.1`. Para clientes externos via túnel e escrita com propostas:

- `--mcp-allow-writes` ou `NOESE_MCP_ALLOW_WRITES=1` habilita as ferramentas de escrita no endpoint `/mcp`.
- `--mcp-token <TOKEN>` ou `NOESE_MCP_TOKEN=<TOKEN>` exige autenticação por `Authorization: Bearer <TOKEN>` ou `/mcp/<TOKEN>` (comparação em tempo constante), permitindo encaminhamento por túnel HTTPS sem liberar as rotas da interface web (`/`, `/mesa`, `/api/v1/*`) para outros `Host`.
- O serviço systemd lê opcionalmente `<data>/mcp.env` via `EnvironmentFile`.

`tools/list` publica os schemas, argumentos obrigatórios, limites e indicações de leitura/escrita. O catálogo também pode ser inspecionado em `GET /api/v1/tools`; essa rota não executa ferramentas. Todas aceitam `window=0|1|2`, padrão 2. Listagens aceitam `limit=1..500`, `offset` e retornam `next_offset`; registros investigativos podem ser filtrados por `inquiry_id`.

Ferramentas de leitura:

| Ferramenta | Resultado |
|---|---|
| `artifacts`, `artifact` | Metadados ou página dos bytes originais, com hash e limites |
| `inquiries`, `inquiry` | Percursos sem questão obrigatória |
| `statements`, `relations`, `opportunities` | Interpretações, ligações e possibilidades candidatas |
| `contributions`, `assessments`, `anchors`, `runs` | Decisões, avaliações atribuídas, âncoras e execuções registradas |
| `record`, `history` | Registro por tipo/ID ou suas versões e eventos |
| `search` | Recuperação textual com proveniência; não certifica afirmações |
| `explore` | Regras transparentes, sem gravar; aceita fonte sem percurso |
| `capabilities` | Capacidades reais e indisponibilidades |
| `reference` | Investigação, registros paginados por grupo e trajetória compartilhada |
| `export_inquiry` | Pacote de investigação retornado, sem gravar arquivo |
| `verify` | Verificação técnica de SQLite, hashes de fontes e âncoras resolvidas |

`artifact` recebe `id`, `offset` e `max_bytes=1..65536`. Texto é devolvido em UTF-8, com `next_offset` sempre em fronteira de caractere. Offset que divide um caractere é recusado. Outros formatos retornam bytes em hex, sem fingir interpretação. Instruções no conteúdo são dados não confiáveis. `verify` pode ler todo o acervo da janela; não atesta autoria ou mérito científico.

Com `--allow-writes`, aparecem também:

| Ferramenta | Efeito |
|---|---|
| `create_inquiry` | Abrir percurso, sem pergunta obrigatória |
| `import_text` | Preservar texto fornecido explicitamente; não abre caminhos fornecidos pelo cliente |
| `create_anchor` | Âncora exata em bytes UTF-8 ou `unresolved` com limitação |
| `propose_note` | Novo enunciado `INFERRED_CANDIDATE`, sem substituir texto existente |
| `propose_relation` | Ligação candidata com justificativa e suporte |
| `propose_opportunity` | Possibilidade candidata; não autoriza ou executa ações |
| `revise_statement` | Edição direta de um enunciado com versão esperada e razão; preservar a anterior |

Contribuições usuais do assistente devem usar `propose_note` ou `propose_opportunity`. O usuário examina, avalia e revisa esses registros no navegador. São registros candidatos próprios do NOESE; não são inseridos automaticamente em um texto canônico. `revise_statement` deve ser usado apenas para edição direta explicitamente solicitada. Essa orientação não é um bloqueio técnico adicional: um cliente com escrita habilitada consegue chamar a ferramenta.

O MCP não expõe autorização de contribuição, execução de capacidade, exclusão sensível, backup, leitura arbitrária de arquivos ou consulta remota. Não há ferramenta para fabricar avaliação humana. Habilitar escrita autoriza somente as operações listadas, sob as regras do serviço.

## Repetição, origem e exclusão

Toda escrita exige `request_id`: UUID hexadecimal com 32 caracteres ou formato com hífens. Reutilize **o mesmo ID e os mesmos argumentos** após perda de resposta. Resultado e efeito de domínio são confirmados na mesma transação. Uma repetição retorna o resultado original, inclusive após reinício; argumentos, operação ou cliente diferentes com o mesmo ID retornam `CONFLICT`. Uma revisão exige ainda `version` e `revision_reason`.

A origem é fixada pelo servidor como `created_by=mcp:<nome declarado>`, `source_type=mcp_client` e `source_ref=mcp:<nome declarado>`. Enunciados e eventos preservam essa origem. O cliente não pode fornecer `created_by` nem promover uma nota para “aprovada”. O nome declarado identifica o canal, sem autenticar uma pessoa. A autoria atribuída a uma fonte é separada da identidade do importador; `import_text` não inventa autor ou data histórica.

A exclusão sensível pelo navegador também remove conteúdo dos resultados de repetição dependentes. Um registro mínimo sem esse conteúdo bloqueia a repetição com `SOURCE_ERASED`, evitando recriar uma fonte apagada por simples retry. Backups anteriores continuam sujeitos à política de retenção própria.

A indicação “leitura por padrão” refere-se às ferramentas: a abertura do acervo ainda inicializa schema e catálogo interno quando necessário. Não é abertura do arquivo SQLite com permissão de sistema somente leitura.

## Limites e validação

Cada mensagem é um objeto JSON-RPC UTF-8 em uma linha, com entrada limitada a 1 MiB e profundidade 64. Exceder 1 MiB encerra o processo com erro em stderr. Notificações não recebem resposta; batches são recusados. O catálogo cabe em uma página. Argumentos desconhecidos, tipos incorretos ou valores fora do schema retornam `-32602`; falhas de domínio como conflito, falta de autoridade ou fonte indisponível retornam `isError=true`. Erros internos não expõem caminhos ou conteúdo na resposta.

Respostas são limitadas a 32 MiB; acima disso retornam `RESULT_TOO_LARGE`. Use leitura paginada ou exportação no navegador. O limite JSON se refere à mensagem serializada, não apenas aos bytes do original. Não há chamadas a modelos ou serviços externos nesse transporte.

`./scripts/verify.sh` executa 12 cenários HTTP e 11 cenários MCP em subprocessos. Inclui compartilhamento HTTP/MCP, lock entre processos, revisões concorrentes, origem, idempotência após reinício, janelas, UTF-8, exclusão e limites de protocolo.

A interoperabilidade também pode ser testada com o SDK oficial TypeScript, instalado somente como ferramenta temporária:

```bash
npm install --prefix /tmp/noese-ui-tools @modelcontextprotocol/sdk@1.32.2
NODE_PATH=/tmp/noese-ui-tools/node_modules node tests/mcp-sdk.cjs
```

Evidência local: `build/mcp/sdk-results.json`. Isso valida o SDK e o processo stdio; a conexão por um aplicativo específico do usuário ainda depende de configurar esse aplicativo.
