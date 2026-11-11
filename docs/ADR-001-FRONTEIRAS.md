# ADR-001 — Fonte, interpretação e ação

Decisão implementada em 2026-11-10 a partir dos documentos compartilhados, tratados como especificação proposta.

Originais são objetos privados endereçados por SHA-256. Importações com bytes iguais geram fontes e ocorrências diferentes; isso não cria evidência independente. A leitura verifica o hash antes de devolver bytes. Fontes, âncoras e ocorrências não têm rotas de revisão. Exclusão sensível explícita é uma exceção documentada.

Interpretações usam tabelas próprias e registros JSON versionados, com ID estável e chave `(id, version)`. Revisar insere uma versão e registra autor, motivo e predecessor. Triggers recusam UPDATE direto. A versão esperada evita perda de edições concorrentes. Referências heterogêneas são resolvidas no serviço antes de gravar; não são chaves estrangeiras SQL dentro do JSON. O esquema é suporte operacional, não ontologia de descobertas. Essa validação precisa acompanhar qualquer nova rota.

Investigações podem nascer sem pergunta. Relações exigem justificativa, referências e suporte ou limitação explícita. Nenhum resultado da busca é promovido automaticamente a verdade histórica.

Possibilidades permanecem propostas, inclusive quando adiadas ou rejeitadas. Contribuições separam proposta, autorização e execução real. A única rota genérica de execução M1 executa `text-compare` local, com dois IDs de âncora exatos no escopo da autorização. Outros recursos locais têm rotas específicas. Ações externas continuam indisponíveis mesmo após registrar autorização.

As janelas J0/J1/J2 são recortes de disponibilidade declarada. Consultas escolhem a última versão disponível na janela, preservando versões anteriores. O programa não demonstra que um pesquisador desconhecia informações fora do corpus.

Alternativas recusadas: integrar núcleos de projetos precedentes, impor métodos analíticos para criar percursos, executar texto importado e atribuir veracidade a inferências pelo sucesso de testes.
