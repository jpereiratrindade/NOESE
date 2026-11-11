# Dependências

| Componente | Versão usada | Origem | Licença | Papel |
|---|---|---|---|---|
| cpp-httplib | v0.28.0 | https://github.com/yhirose/cpp-httplib/tree/v0.28.0 | MIT, `third_party/httplib.LICENSE` | HTTP/1.1 local |
| nlohmann/json | v3.12.0 | https://github.com/nlohmann/json/tree/v3.12.0 | MIT, `third_party/json.LICENSE` | Contratos e campos extensíveis |
| SQLite | sistema; verificada 3.51.2 | https://sqlite.org | Domínio público | Banco, FTS5, JSON, backup |
| OpenSSL Crypto | sistema; verificada 3.5.9 | https://openssl.org | Apache-2.0 nessa versão | SHA-256 e aleatoriedade |

SHA-256 dos headers incluídos:

```text
9e5cafaa227dad5c10e837990a736cecaff03258f50d37067cedeee1dba5d344  third_party/httplib.h
aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63  third_party/json.hpp
```

Critério: HTTP local e serialização JSON eram capacidades ausentes em um workspace vazio. Implementá-los manualmente ampliaria a superfície de erro; bibliotecas pequenas, independentes, com versões fixas e licenças preservadas atendem ao monólito. Não há dependência de outros projetos do portfólio. As bibliotecas recebem dados locais em memória; não conectam serviços externos. Esta seleção e os testes adversariais não equivalem a auditoria completa de segurança dos fornecedores.
