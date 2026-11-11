# LAB-001 — Reprodução e avaliação

Data local da implementação: 2026-11-10, America/Sao_Paulo. Instantes operacionais são armazenados em UTC; não são datas das falas.

Fonte: ZIP compartilhado `NOESE-CAVEMAN-FULL-LAB001-v0.1.0.zip`. Corpus SHA-256 `e7fb9bc53c311de72bb205fb36ecb7d92924223d49fad0da3bc88fc519c90901`, 21 excertos curados, `PARTIAL_EXCERPTS`. Não representa exportação integral nem autentica falas. Os documentos originais permanecem em pastas privadas; `scripts/import_lab.py` confere o manifesto sem executar o texto.

1. Execute `./scripts/verify.sh`. A suíte usa bases temporárias; o acervo de uso permanece vazio.
2. Execute `./scripts/run.sh --data var/lab-humano`. Escolha J0 e importe LAB-001. São disponibilizados somente X01–X10.
3. Abra uma investigação sem hipótese obrigatória. Leia, selecione um trecho, anote uma pergunta e registre uma interpretação alternativa. Verifique o retorno à exata fonte e autoria atribuída.
4. Examine possibilidades. A regra `question.no_assessment.v1` procura literalmente `?` em enunciados e verifica avaliações vinculadas. Isso indica apenas falta de avaliação explícita no recorte examinado. `coverage.not_indexed.v1` indica originais preservados sem índice textual. `source.question.review.v1` encontra a primeira interrogação literal na fonte selecionada e devolve o trecho com offsets verificáveis, sem afirmar que a pergunta está sem resposta. É possível explorar sem percurso aberto; um percurso só é criado ao salvar um candidato. Nenhuma regra contém nomes do episódio PRAXIS ou a resposta esperada do laboratório.
5. Registre uma oportunidade que considerar pertinente, ou documente que nada foi sustentado. Avalie falsos positivos, custo e importância para a questão; não conte sugestões como ganho científico.
6. Passe a J1 e importe os excertos adicionais. A duplicidade de nomes continua sendo relato da conversa: o pacote não inclui READMEs externos independentes e o software não os consulta. Confronte proposta, objeção e reformulação por ligações explícitas.
7. Se útil, anote dois fragmentos e use **Comparar trechos**. Confira o escopo antes de autorizar. Examine a saída literal e registre avaliação sobre consequência, não contribuição ou inconclusão. A comparação não prova mudança conceitual ou causalidade.
8. Passe a J2. Critique também os critérios deste protocolo e a própria especificação; preserve objeções e revisões. Registre uma contribuição externa apenas como proposta, caso pertinente; sua execução permanece indisponível.
9. Volte a J0. Fontes posteriores e revisões de J2 não podem aparecer como informação de J0. O recorte reflete disponibilidade declarada, não onisciência histórica do pesquisador.
10. Exporte o percurso. Confira fontes e versões no pacote e use `scripts/unpack_export.py` para reabri-lo e verificar os bytes. Crie backup, restaure em diretório novo e reabra a investigação após reiniciar.

Registro de avaliação humana pendente: pertinência das possibilidades; alternativas relevantes omitidas; erros de atribuição; tempo para recuperar uma trajetória; utilidade do confronto; custo; consequências observáveis da contribuição; riscos de enquadrar a investigação pelos registros disponíveis. Preserve inclusive resultado negativo.

A verificação em `tests/browser.cjs` percorre a UI com os excertos reais quando presentes, mas insere enunciados **de teste** em base temporária. Suas avaliações são verificações técnicas automatizadas; não são avaliações científicas feitas pelo pesquisador. Capturas e exportações de teste ficam somente em `build/browser/`.
