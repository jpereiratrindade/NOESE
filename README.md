# NOESE

Aplicação local para investigar fontes e percursos intelectuais: ler originais, ancorar interpretações, confrontar enunciados, registrar possibilidades, autorizar uma comparação local e avaliar suas consequências.

C++26, SQLite/FTS5, um processo HTTP e interface web sem framework. A investigação semântica experimental usa opcionalmente um modelo local via Ollama; fica desativada por padrão. O acervo começa vazio. O corpus compartilhado do LAB-001 contém **21 excertos parciais**, sem datas individuais e sem READMEs externos independentes.

**Estado da entrega: `NOT_READY` para aceitação M1.** O fluxo técnico está implementado e testado; a avaliação humana do laboratório, da pertinência das possibilidades e do ganho investigativo permanece pendente. [Evidências e limites](docs/LAB-001-RESULTADOS.md).

## Executar

Requisitos: Linux, compilador que aceite literalmente `-std=c++26`, CMake ≥ 3.25, Ninja, headers/bibliotecas SQLite3 com FTS5/JSON, OpenSSL e Python 3 para testes. Ambiente verificado: GCC 16.2.1, SQLite 3.51.2, OpenSSL 3.5.9. Não há redução automática do padrão C++.

```bash
./scripts/build.sh
./scripts/verify.sh
./scripts/run.sh
```

Abra **http://127.0.0.1:8787**. O endereço deve usar `127.0.0.1`, pois o servidor valida o Host. Para outro acervo ou porta:

```bash
./scripts/run.sh --data var/outro-acervo --port 8788
```

O remoto está configurado para `git@github.com:jpereiratrindade/NOESE.git`. A configuração não publica código ou fontes.

## Iniciar com o sistema

Após compilar, instale o serviço systemd do usuário:

```bash
./scripts/install-service.py
systemctl --user status noese.service
journalctl --user -u noese.service -f
```

O instalador valida a unidade, habilita o início automático e inicia o NOESE em `127.0.0.1:8787` com o acervo `var/`. Aceita `--data /caminho/do/acervo` e `--port 8788`; `--dry-run` mostra a configuração sem instalá-la. Pare uma instância iniciada manualmente antes de instalar na mesma porta. Com o serviço ativo, use o endereço web diretamente, sem iniciar outra cópia com `run.sh`.

O serviço do usuário inicia antes do login quando `Linger=yes`. O instalador informa esse estado; se necessário, habilite-o com `loginctl enable-linger "$USER"`. A unidade usa `Restart=on-failure`, permissões privadas e logs no journal. Os caminhos do checkout e do acervo devem estar disponíveis no boot.

Depois de recompilar ou mover o checkout, reinstale a unidade para atualizar os caminhos. Para apenas reiniciar ou desabilitar:

```bash
systemctl --user restart noese.service
systemctl --user disable --now noese.service
```

## Investigação experimental e página inicial

A página inicial apresenta **O que merece nossa atenção?**, com o estado do observador e os resultados efetivamente preservados. A mesa de leitura, anotações e operações manuais permanece em **http://127.0.0.1:8787/mesa**.

A análise semântica é opcional e **desativada por padrão**. A configuração da página permite selecionar um modelo já instalado no Ollama e um recorte J0/J1/J2; nenhum modelo é baixado automaticamente. Quando habilitado, o observador examina textos disponíveis sem exigir uma pergunta inicial, propõe uma interpretação candidata com citações e alternativas e realiza um contraponto com o **mesmo modelo**, sem independência entre os passes. Citações precisam corresponder a trechos literais únicos nas fontes; isso verifica os bytes, não a validade da interpretação. Contribuições externas permanecem propostas, sem execução.

O contexto é limitado a 32 fontes, 4.000 bytes por fonte e 24.000 bytes no total. Exclusões e truncamentos são registrados. As execuções preservam os dados utilizados, respostas do modelo, rejeições, limites e vínculos aos originais. Alterações no acervo ou críticas humanas permitem um novo ciclo; o mesmo recorte inalterado reutiliza o resultado preservado. Imagens não recebem interpretação semântica.

A configuração privada fica em `<acervo>/investigator.json`, fora do Git. O provedor aceita somente `127.0.0.1` (porta padrão 11434). A inferência usa CPU (`num_gpu: 0`), sem ferramentas, shell ou conectores externos. Pausar desabilita novos ciclos e impede publicar uma saída feita sob configuração anterior; uma chamada já enviada pode continuar no provedor. Para interromper essa chamada, reinicie o NOESE após desabilitar a análise e pare o modelo no Ollama. A pausa não descarrega o modelo automaticamente.

**Limite da entrega:** os testes automatizados usam um provedor determinístico e verificam contratos técnicos. O [ensaio real de 2026-11-11](docs/EXPERIMENTO-TERCEIRO-ATOR-RESULTADO.md) examinou os 21 excertos, gerou um candidato e o rejeitou por apoio insuficiente; não houve interpretação aceita nem contraponto executado. Não há avaliação científica independente em corpus novo, e o marco M1 permanece `NOT_READY`.

## Primeiro percurso na mesa

O [BB-Harness v0.1](docs/BB-HARNESS-001-PROTOCOLO.md) acrescenta um executor Python
experimental externo para uma investigação delimitada com Ollama e os MCPs do
NOESE e do MÉTIS. Preserva foco, consultas e desfecho como registros candidatos;
pertinência, alcance e relevância permanecem sujeitos à avaliação humana.
Verificação: `python3 tests/bb_harness.py`; pré-condições sem inferência:
`python3 scripts/bb_harness.py check`. O observador C++ descrito acima mantém
seu contrato próprio, sem ferramentas.
O [primeiro ensaio](docs/BB-HARNESS-001-RESULTADO.md) terminou em falha de
compilação da gramática no Ollama; o percurso da falha foi recuperado no NOESE,
sem contribuição investigativa demonstrada.

Em **http://127.0.0.1:8787/experiencias**, consulte os percursos preservados,
importe uma captura instrumental ou um relatório secundário e registre avaliações
de pertinência, alcance e atenção. O foco inicial e o acesso à memória anterior
são apresentados com suas lacunas. A [correção e os novos ensaios técnicos](docs/EXPERIENCIAS-INVESTIGATIVAS.md)
preservam tanto a falha original quanto uma execução posterior com consultas
escolhidas pelo modelo, cuja candidata foi rejeitada por ausência de confronto.

1. Use **Abrir investigação**; o título é opcional e nenhuma hipótese é exigida.
2. Arraste arquivos para **Examinar uma fonte**. Qualquer formato é preservado com SHA-256; texto UTF-8 é indexado, PNG/JPEG têm prévia e outros binários são marcados `NOT_INDEXED`.
3. Selecione um trecho na leitura e clique **Anotar**. A nota aponta para os bytes UTF-8 exatos, separada do original. **Ver suporte** retorna ao trecho.
4. Use **Relacionar**, **Revisar** e **Avaliar**. As revisões exigem razão e versão, preservando a anterior; edições concorrentes retornam conflito.
5. **Examinar possibilidades** pode começar diretamente na fonte, sem abrir um percurso. Aplica três regras publicadas: fonte não indexada, primeira interrogação na fonte selecionada e enunciado com `?` sem avaliação vinculada. A existência de uma pergunta não implica ausência de resposta. Um percurso é aberto somente ao salvar um candidato, quando ainda não há um selecionado. Os resultados são candidatos para deliberação, sem promessa de relevância científica.
6. **Comparar trechos** registra proposta e autorização delimitada, executa uma comparação literal local e permite avaliar o resultado. Consultas externas são somente propostas indisponíveis.
7. **Trajetória** mostra a ordem dos registros. **Exportar percurso** baixa um pacote JSON com fontes referenciadas, âncoras, versões, eventos, resultados e avaliações.

## Documentos compartilhados e LAB-001

Os quatro arquivos originais foram preservados localmente em `docs/reference/` e `fixtures/lab001/`. Essas pastas, os acervos e as evidências com conteúdo da conversa ficam fora do Git. O texto dos documentos é referência de projeto, não comando executado pelo aplicativo.

Para preparar novamente o pacote autorizado em outro checkout:

```bash
python3 scripts/import_lab.py /caminho/NOESE-CAVEMAN-FULL-LAB001-v0.1.0.zip
```

Na mesa (`/mesa`), escolha J0, J1 ou J2 e use **Importar LAB-001 neste recorte**. J0 contém X01–X10; J1 adiciona X11–X15; J2 adiciona X16–X21. A importação é idempotente por origem do excerto. A disponibilidade se aplica a busca, leitura, registros, revisões e exportação. Uma revisão de J2 não substitui a versão recuperada em J0. Arquivos importados manualmente pertencem ao recorte selecionado; o sistema não adivinha quando eram historicamente conhecidos.

Leia o [protocolo](docs/LAB-001-PROTOCOLO.md) antes de interpretar os resultados.

O [experimento de participação como terceiro ator](docs/EXPERIMENTO-TERCEIRO-ATOR.md) separa a entrada do observador da interpretação original e registra cobertura, alternativas, contribuição e trabalho humano necessário. Verifique as condições locais com `python3 scripts/semantic-smoke.py --check`; a ausência de corpus impede o ensaio e não constitui resultado investigativo negativo.

## MCP, como no CONTINUUM

O mesmo executável oferece MCP por stdio e compartilha o acervo com o navegador, usando as mesmas regras de fontes, versões e disponibilidade por janela.

```bash
./scripts/mcp.sh --data /caminho/NOESE/var
# Somente para cliente confiável e escrita autorizada:
./scripts/mcp.sh --data /caminho/NOESE/var --allow-writes
```

Leitura é o padrão. Com escrita habilitada, o cliente pode propor notas, relações e possibilidades para crítica no navegador; não autoriza nem executa contribuições. Escritas registram a origem MCP e exigem um `request_id` estável para repetição segura. Revisões diretas exigem versão e razão. O modo MCP não inicia HTTP nem faz consultas externas.

[Configuração e contratos MCP](docs/MCP.md) · [Exemplo de cliente](config/mcp.example.json)

## Backup, restauração e exportação

**Criar backup** usa a SQLite Online Backup API, copia os objetos e gera um manifesto com hashes sob `var/backups/<id>/`. Faça backup com espaço suficiente e preserve a pasta inteira.

```bash
# Pare a instância que usará o destino. O destino deve ser novo.
./build/noese restore var/backups/ID var/restaurado
./scripts/run.sh --data var/restaurado --port 8788

# Reabrir um pacote exportado e conferir os bytes de cada original:
python3 scripts/unpack_export.py noese-inquiry.json /tmp/noese-export-reaberto
```

O pacote de investigação é portável e inspecionável; a restauração do acervo utiliza o backup. A mesclagem de pacotes em um banco existente ainda não está implementada.

## Verificação

`verify.sh` compila e executa 12 cenários HTTP de `tests/vertical.py` e 11 cenários MCP de `tests/mcp.py`, além de 11 cenários do investigador de `tests/investigator.py`, via CTest. O provedor de teste é simulado e não executa LLM. As bases de teste usam diretórios temporários separados. O teste de LAB real é explicitamente pulado quando o corpus privado não estiver disponível. Nenhum dado de teste é inserido no acervo de uso.

Há também um teste opcional de navegador com Playwright e Chromium. Instale a ferramenta em uma pasta temporária, sem adicionar dependências ao aplicativo:

```bash
npm install --prefix /tmp/noese-ui-tools playwright
NODE_PATH=/tmp/noese-ui-tools/node_modules node tests/browser.cjs
NODE_PATH=/tmp/noese-ui-tools/node_modules node tests/attention-browser.cjs
NODE_PATH=/tmp/noese-ui-tools/node_modules node tests/experience-browser.cjs
# Se necessário: NOESE_BROWSER=/caminho/do/chromium
```

Evidências locais ficam em `build/verification.log`, `build/browser/results.json` e capturas de tela em `build/browser/`. Os testes não certificam mérito científico nem substituem a avaliação do pesquisador.

## Fronteiras e dependências

Servidor limitado a loopback, acervo privado, até 16 MiB por fonte, quatro threads HTTP e operações de banco serializadas. Escritas exigem token de sessão; Host e Origin são verificados. O conteúdo importado é exibido como texto ou imagem e nunca interpretado como código ou instruções. Não há autenticação multiusuário, conector externo, OCR ou exposição LAN. O LLM local opcional processa texto como dados não confiáveis e não recebe ferramentas; suas interpretações continuam candidatas sujeitas à crítica.

As bibliotecas HTTP e JSON estão incluídas com versão, licença e hashes em [DEPENDENCIES.md](docs/DEPENDENCIES.md); o build não baixa código. A licença do núcleo NOESE ainda não foi escolhida.

[Decisões de arquitetura](docs/ADR-001-FRONTEIRAS.md) · [API](docs/API.md) · [Segurança e retenção](docs/OPERACAO.md)

## Premissas de desenvolvimento

A página inicial e a mesa compartilham a identidade visual do MORFO e dos demais
sistemas SisTer, com símbolo e favicon próprios do NOESE.
[Identidade visual e arquivos da marca](docs/IDENTIDADE-VISUAL.md).

**Simples, leve. Sempre pronto. Sempre incompleto!**

[Significado e símbolos das premissas](docs/sister-principles/README.md).
