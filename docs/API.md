# API local v1

Base: `http://127.0.0.1:8787/api/v1`. JSON nas respostas, salvo conteúdo/original. `GET /session` fornece token da sessão. Escritas exigem `X-NOESE-Token`. O token muda ao reiniciar o processo; não é identidade multiusuário.

Todas as leituras aceitam `window=0|1|2`, padrão 2. Listagens aceitam `limit` (1–500 por convenção de uso; zero retorna vazio), `offset` e, para registros investigativos, `inquiry_id`. Busca limita `limit` a 100. Referências usam `{ "type": "anchors", "id": "..." }`.

| Rota | Operação |
|---|---|
| `GET /health` (fora de `/api/v1`) | Readiness técnico, SQLite e limite do marco |
| `GET /artifacts` | Fontes e limites de processamento |
| `POST /artifacts` | Multipart `file` + JSON opcional `metadata`; bytes preservados |
| `GET /artifacts/{id}` | Metadados e hash |
| `GET /artifacts/{id}/content` | Texto UTF-8 ou PNG/JPEG; 422 para prévia indisponível |
| `GET /artifacts/{id}/original` | Download binário com verificação de SHA-256 |
| `DELETE /artifacts/{id}` | `confirm_sha256`; exclusão sensível e registros dependentes |
| `GET /occurrences` | Ocorrências de importação, sem independência presumida |
| `POST /anchors` | `artifact_id`, `start`, `end` (bytes UTF-8), `quote` opcional; ou `unresolved=true` + `limitation` |
| `GET /search?q=texto` | FTS5, escopo, metadados e localização; correspondência normalizada pode exigir seleção manual |
| `GET, POST /inquiries` | Percursos sem questão obrigatória |
| `GET, POST /statements` | Enunciados e suporte/limite explícito |
| `GET, POST /relations` | `references` com ≥2 elementos, `rationale`, `basis` ou limitação |
| `GET, POST /opportunities` | Possibilidades candidatas, incluindo `rejected` e `deferred` |
| `GET, POST /contributions` | Propostas e decisões de contribuição; estado inicial `proposed` |
| `GET, POST /assessments` | `target`, texto, suporte e apreciação humana |
| `GET, POST /inquiry_members` | Associar `reference` ao percurso sem interpretação obrigatória |
| `PUT /{recurso}/{id}` | Nova versão para recursos investigativos; exige `version` atual e `revision_reason` |
| `GET /history/{tipo}/{id}` | Versões e eventos disponíveis na janela; para investigação agrega eventos vinculados |
| `GET /explore/{inquiry_id}` | Regras transparentes de cobertura/perguntas; até 500 itens por categoria |
| `GET /capabilities` | Propósito, contratos, disponibilidade, versão e limites |
| `POST /runs` | Comparação local autorizada: `contribution_id`, `left_anchor`, `right_anchor` |
| `GET /runs` | Tentativas de comparação efetivamente realizadas |
| `GET /export/inquiries/{id}` | JSON `NOESE-INQUIRY-1`, fechamento de referências, originais em hex, hashes e versões |
| `POST /lab/import` | Importar excertos reais disponíveis até a janela escolhida |
| `POST /backups` | Backup consistente no diretório privado do acervo |

Um enunciado exige `inquiry_id`, `text` e `basis` não vazio; sem suporte, exige `basis_unresolved=true` e `limitation`. Referência direta a uma fonte em enunciados deve passar por uma âncora. Referências podem apontar para enunciados; isso não os transforma em evidência independente. Autor da operação padrão: `local_user`, distinto da autoria atribuída à fonte.

Autorizar uma contribuição exige revisar uma proposta existente para `authorized` com `authorization_scope`. Para `text-compare`, o escopo é uma string JSON com os IDs exatos `left_anchor` e `right_anchor`. `/runs` verifica esses IDs, a disponibilidade e o estado antes de executar. A execução grava resultado e altera a contribuição para `performed` em transação. Avaliação continua separada. Nenhuma rota executa consultas externas.

Erros estruturados: `INVALID_INPUT`, `NOT_FOUND`, `UNRESOLVED_ANCHOR`, `AUTHORITY_REQUIRED`, `CAPABILITY_UNAVAILABLE`, `CONFLICT`, `INCOMPLETE_SOURCE`, `INTEGRITY_FAILURE`, `INTERNAL_ERROR`. Os erros JSON não devolvem detalhes internos; o log local contém diagnóstico.

Limites: até 16 MiB por arquivo, requisição HTTP até 17 MiB, exportação até 9.999 registros por tipo antes do fechamento de referências. Pacotes com muitos originais usam memória proporcional ao conteúdo; o M1 é destinado a acervos locais pequenos.
