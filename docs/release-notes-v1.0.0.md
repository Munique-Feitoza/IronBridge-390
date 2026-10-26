# IronBridge-390 v1.0.0

## Visão geral

Primeira versão demonstrável do IronBridge-390: uma ponte entre uma API HTTP
em JSON/UTF-8 e uma aplicação COBOL orientada a copybook, com conversão de
EBCDIC, valor monetário em COMP-3 e transporte TCP com frame binário.

## Entregas da versão

- API HTTP para criação de transações.
- Gateway TCP em C entre a API e o backend legado.
- Backend de referência em C e backend COBOL compatível com GnuCOBOL.
- Codificação e decodificação de campos `PIC X`, `PIC 9` e `COMP-3`.
- Leitura do layout do copybook e mapeamento de nomes alternativos.
- Validação de identificador, conta, valor, moeda e operação.
- Idempotência opcional por `Idempotency-Key` com SQLite.
- Autenticação Bearer e TLS opcionais para execução local.
- Endpoints de saúde e prontidão (`/health`, `/ready` e `/metrics`).
- Limites de tamanho, timeout, concorrência e proteção contra entrada
  fragmentada ou inválida.
- Testes unitários, integração REST, segurança, robustez e sanitizers.
- Verificação reproduzível com zCOBOL/z390.
- Perfil seguro para validação do Hercules/TK5 e execução de JCL de prova.
- Documentação técnica, operacional e evidências de execução.

## Como validar

```bash
make test
make hardening-test
make asan-test
make schema
make z390-test
```

Os testes do Hercules dependem da instalação local do emulador e de uma
imagem compatível. Quando disponíveis, podem ser executados com:

```bash
make hercules-check
make hercules-mvs-check
make hercules-jcl-check
```

## Escopo conhecido

Esta versão mantém um contrato físico de 42 bytes para a transação. O parser
de copybook valida esse contrato e aceita aliases por arquivo de mapeamento,
mas ainda não pretende interpretar todos os recursos da linguagem COBOL, como
`REDEFINES`, `OCCURS`, grupos aninhados arbitrários ou todos os formatos
`COMP`.

A execução comprovada em MVS/TK5 cobre o IPL e a submissão de JCL de prova. A
execução do serviço completo dentro de z/OS, com CICS, IMS, Db2 ou VSAM reais,
depende de um ambiente IBM compatível e deve ser registrada separadamente.

## Próximo ciclo

- ampliar o parser para estruturas COBOL adicionais;
- adicionar testes de carga e falhas de dependências ao CI;
- documentar uma implantação em ambiente IBM Z de desenvolvimento;
- separar adaptadores CICS, IMS, Db2 e VSAM em módulos específicos.
