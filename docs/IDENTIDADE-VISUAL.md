# Identidade visual do NOESE

O NOESE compartilha a composição visual do MORFO e do EVIDA: navegação lateral
marinho, cabeçalho claro, cartões brancos e fundo azul suave. A referência foi
inspecionada nos arquivos locais `MORFO/web/style.css`, `MORFO/web/index.html`,
`evida/web/app.css` e `CONTINUUM/docs/05-IDENTIDADE-VISUAL.md`.

A paleta usa marinho `#071e38`, azul de ação `#1a7dc4`, ciano `#3cc7e7`,
âmbar `#ffc06a` e fundo `#edf2f8`. A tipografia usa a pilha local
Plus Jakarta Sans / Inter / system-ui, sem baixar fontes. Citações e originais
mantêm sua distinção visual em relação às interpretações e aos controles.

O símbolo próprio **foco aberto** reúne dois percursos curvos, um núcleo de
atenção e aberturas para continuidade. O ciano representa os percursos; o âmbar
destaca o foco. Ele não sinaliza aprovação científica nem estado do serviço.
O SVG original está em `web/noese-symbol.svg`; `web/favicon.svg` adapta o desenho
para o fundo marinho da aba do navegador. Ambos são vetoriais, sem imagens ou
recursos externos embutidos.

`web/identity.css` e `web/identity.js` compartilham a identidade entre `/` e
`/mesa`. A navegação lateral contém apenas os dois espaços existentes; seleção
de recorte, configurações, leitura, anotação e avaliação conservam seus fluxos.
Em telas estreitas, o menu oferece fechamento por Escape, retorno de foco e
contenção da navegação por teclado. Os princípios SisTer permanecem visíveis
na navegação. O servidor publica os novos arquivos por uma lista explícita e
serve os SVGs com `image/svg+xml`.

Verificação: suíte CTest e os dois testes de navegador existentes, com checagens
compartilhadas de marca, favicon, ausência de rolagem horizontal em sete larguras
(320–1440 px) e navegação móvel por teclado. Capturas ficam em `build/browser/`
e `build/attention-browser/`. Esses testes avaliam os contratos técnicos; a
avaliação visual e de uso pelo pesquisador permanece aberta.
