# LAB-001 — Evidências da implementação

Estado: **`NOT_READY` para aceitação M1**. Existe aplicação local funcional com P0/P1 implementados nas fronteiras descritas abaixo. A pertinência científica das oportunidades, a usabilidade pelo pesquisador e o ganho real sobre a investigação ainda não foram avaliados por uma pessoa. Não há certificação de descoberta autônoma.

Verificação em 2026-11-10 (America/Sao_Paulo). GCC 16.2.1, `-std=c++26` explícito, SQLite 3.51.2, OpenSSL 3.5.9. CMake/Ninja; build sem downloads. Logs locais: `build/verification.log`, `build/compile_commands.json`, `build/browser-tests.log`, `build/browser/results.json`. Fontes e saídas privadas são ignoradas pelo Git.

| Requisito / gate | Código ou rota | Execução / evidência | Limitação | Estado |
|---|---|---|---|---|
| G-01 C++26 | CMake, `scripts/verify.sh` | Flag conferida no compile_commands; CTest com 11 cenários | Suporte do compilador é experimental; não usa reflection/contracts | Verificado tecnicamente |
| G-02 Original | Store::object; `/artifacts` | Bytes e SHA-256 reabertos; contextos distintos com mesmo hash | Hash não autentica autoria nem verdade | Verificado tecnicamente |
| G-03 Âncora | Service::anchor; UI seleção | UTF-8, fragmento divergente e intervalos inválidos testados; browser volta ao mark | Binários sem interpretação usam unresolved | Verificado tecnicamente |
| G-04 Autoria e tempo | Fonte + occurrences | Autor atribuído separado do importador; timestamps de falas nulos no LAB | Corpus sem metadados históricos integrais | Verificado no escopo disponível |
| G-05 Investigação aberta | `/inquiries` | Criação sem pergunta por HTTP e browser | Utilidade do percurso exige avaliação humana | Verificado tecnicamente |
| G-06 Revisão | Store::save; `/history` | Versões preservadas; edição simultânea retorna 200/409 | Operação local de um usuário | Verificado tecnicamente |
| G-07 Possibilidade e contribuição | `/opportunities`, `/contributions`, `/runs` | Proposta → autorização exata → comparação real → avaliação de teste; rejeição/adiamento preservam versões | Não demonstra ampliação científica | Verificado tecnicamente; mérito pendente |
| G-08 Fontes externas | capabilities external-lookup | Execução externa retorna CAPABILITY_UNAVAILABLE mesmo com autorização registrada | Nenhum README independente foi importado | Indisponibilidade explícita |
| G-09 Crítica reflexiva | Statements/assessments e regras gerais | Infraestrutura permite criticar especificação e revisar; detector não contém nomes do episódio | Contraste humano da própria implementação ainda não realizado | Pendente de avaliação humana |
| G-10 UI e persistência | `web/`, `tests/browser.cjs` | Importar, ler, anotar, relacionar, comparar, avaliar e exportar; servidor reiniciado; 9 verificações Chromium sem erros JS | Revisão humana de usabilidade e acessibilidade completa pendente | Verificado tecnicamente |
| G-11 Segurança | Server.cpp, Store.cpp, UI textContent | Host/Origin/token, caminho hostil, upload > limite, HTML/script e prompt malicioso testados | Não é auditoria completa de dependências ou segurança | Verificado nos casos descritos |
| G-12 Recuperação | `/backups`; `noese restore` | Backup, restauração e leitura real; objeto adulterado recusado | Cópias anteriores precisam de retenção/exclusão próprias | Verificado tecnicamente |
| G-13 Janelas | Store::get/list; import_lab | 10/15/21 excertos; J0 não vê fonte ou revisão J2; exportação J0 mantém versão antiga | Disponibilidade manual não prova ignorância histórica | Verificado tecnicamente |
| P1 Comparação | `text-compare` | Duas âncoras reais; prefixo e diferenças literais; resultado persistente | Semântica, influência e causalidade não inferidas | Implementado e testado |
| P1 Oportunidades | `/explore` | Pergunta sem avaliação e binário não indexado geram candidatos com regra e suporte | Regras simples, até 500 itens; pertinência não demonstrada | Implementado; avaliação pendente |
| Exclusão sensível | `/artifacts/{id}` DELETE | Original, índice, notas e versões dependentes removidos; SQLite não contém token apagado | Cópias independentes e apagamento físico ficam fora da garantia | Verificado no caso testado |
| Exportação reaberta | `/export/inquiries`; unpack_export.py | Fechamento de referências, fontes com hashes e versões; pacote JSON inspecionável | Não mescla pacotes em banco existente | Implementado |

O teste de navegador usa uma investigação temporária e enunciados de teste sobre excertos reais. O registro de uma “avaliação humana” é a verificação técnica de um tipo de registro; não representa a opinião ou aprovação do pesquisador. Os 21 excertos permanecem `PARTIAL_EXCERPTS`, sem horário individual. Não foi feita consulta externa nem simulação de resposta do GitHub.

As regras de possibilidades são desmontáveis: fonte com `processing=NOT_INDEXED`; enunciado contendo `?` e sem assessment cujo target seja seu ID. Um resultado negativo é permitido. A cobertura de 500 itens e o universo local examinado são explicitados na resposta. Nenhuma regra tenta antecipar o episódio conhecido por nomes ou conclusões da rubrica.

Faltam para aceitar `READY / NOESE_M1`: executar o protocolo com o pesquisador, julgar pertinência e alternativas dos candidatos, documentar consequências contrastáveis e revisar a usabilidade. A transcrição integral, fontes externas independentes, OCR, semântica multimodal, mesclagem de pacotes e execução externa não estão disponíveis nesta versão.
