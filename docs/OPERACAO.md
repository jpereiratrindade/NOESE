# Operação, autoridade e retenção

O processo aceita conexões somente em `127.0.0.1`, sem parâmetro para exposição LAN. Host deve corresponder ao endereço e porta. Origin externo, Fetch-Site cross-site e escritas sem token são recusados. CSP, nosniff e bloqueio de frames reduzem execução de conteúdo. As rotas estáticas são uma lista fechada; o diretório de objetos não é servido como filesystem. Nomes com separadores e hashes fora de SHA-256 hexadecimal são recusados.

Qualquer arquivo é preservado como bytes. A detecção é deliberadamente pequena: assinaturas PNG, JPEG e PDF; caso contrário, texto UTF-8 sem NUL ou binário opaco. Não equivale a validação estrutural de imagens ou extração universal. HTML, scripts e instruções em fontes são renderizados com `textContent`, sem execução. Originais são baixados como attachment com nome gerado. A abertura de um original em outro aplicativo está fora do controle do NOESE.

O acervo utiliza diretório 0700 e arquivos novos limitados por umask 0077. A aplicação não cifra o disco. Autoridades são as do usuário local que iniciou o processo: não há isolamento contra outros processos da mesma conta. Não se deve usar este servidor como serviço remoto ou multiusuário.

Reter fontes enquanto necessárias à investigação é uma decisão humana; não há política automática de expiração. **Proveniência → Excluir fonte e registros dependentes** exige confirmar o SHA-256. Remove originais não compartilhados por outra ocorrência, índices e registros que dependam da fonte; elimina também suas versões e eventos com conteúdo. Preserva somente um evento sem o conteúdo apagado e executa limpeza SQLite/WAL. A política conservadora considera menções ao ID como dependência. Uma cópia com o mesmo hash em outra ocorrência mantém o objeto até que essa ocorrência seja excluída.

Backups e exportações são cópias independentes: exclua-as separadamente quando necessário. Não há promessa de apagamento físico em SSD ou de cópias feitas por terceiros. A interface explica essa fronteira antes da exclusão.

Backup exige espaço disponível para banco e todos os objetos. A SQLite Online Backup API é usada com operações de banco e importação serializadas, seguida de cópia dos objetos e manifesto. Restauração verifica hash do banco e de cada objeto antes de criar um destino novo. O teste automatizado restaura e reabre um original; também recusa um objeto de backup adulterado.

Os arquivos de referência compartilhados, o corpus e os dados em `var/` são ignorados pelo Git. Não há push, publicação ou envio de conteúdo realizado pelo aplicativo. Regras de acesso a fontes externas são catálogo/proposta; não há conector implementado.
