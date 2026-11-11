# ADR-002 — Exploração antes da abertura de um percurso

Um retorno real de uso mostrou que “Examinar possibilidades” devolvia “Abra uma investigação para registrar este percurso” mesmo com uma fonte selecionada. A exigência impunha uma etapa administrativa antes da exploração.

A exploração é agora uma leitura: `/explore` funciona sem investigação e aceita uma fonte explícita. Não grava enunciados, âncoras, possibilidades ou percursos. A regra adicional identifica a primeira interrogação literal da fonte selecionada, publica o trecho e seus offsets e declara que não verificou se existe resposta. Não se trata de análise semântica ou de descoberta de lacuna científica.

Ao decidir salvar um candidato, o usuário pode rever sua formulação. Somente então a interface cria um percurso aberto se necessário e registra uma âncora verificável quando há trecho. Cancelar ou apenas examinar não cria registros. Sem candidatos, o painel mostra o resultado e os limites das regras. Trocar a fonte ou investigação limpa candidatos de contextos anteriores.

A regra textual é simples e substituível. A fronteira que deve permanecer é permitir exploração sem hipótese ou percurso obrigatório, separando leitura, proposta, decisão e execução.
