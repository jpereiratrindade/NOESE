# ADR-003 — MCP stdio e operações compartilhadas

Pedido do usuário: adotar a mesma abordagem MCP usada no CONTINUUM. A inspeção do projeto de referência mostrou stdio, ferramentas de leitura por padrão, escrita explícita, schemas, origem do cliente e persistência comum com a interface.

Foi implementado um adaptador próprio C++26 para MCP 2025-11-25. `Operations` define contratos e chama a mesma `Service` usada pela API; `Mcp` cuida apenas de sessão, JSON-RPC e enquadramento stdio. O catálogo é inspecionável pelo HTTP. Não foi importado código nem núcleo do CONTINUUM; o objeto investigativo permanece independente.

Propostas criam registros candidatos separados; edição direta de enunciado permanece possível com escrita habilitada, versão e razão. A origem MCP é atribuída pelo servidor. Autorizações e execuções de contribuições não são ferramentas MCP. Não há leitura de caminhos locais fornecidos nos argumentos.

Um segundo processo sobre o mesmo acervo tornou insuficiente o mutex de threads existente. O lock passou a combinar mutex recursivo e flock por operação, cobrindo também objetos, exportação e backup. SQLite continua usando WAL. Transações internas usam savepoints quando há transação externa, permitindo confirmar chamada idempotente e efeito do domínio juntos.

A migração 002 mantém request_id, assinatura e resultado das escritas. Exclusão sensível elimina conteúdo desses resultados e conserva apenas marcador revogado, para não ressuscitar fonte apagada em um retry. A abertura do banco pode inicializar schema mesmo quando só ferramentas de leitura são expostas.

Não foi acrescentado transporte HTTP remoto, OAuth, SaaS ou SDK de execução obrigatório. O SDK oficial é usado somente na verificação opcional de interoperabilidade. Configurar um aplicativo cliente é uma etapa distinta, não executada automaticamente.
