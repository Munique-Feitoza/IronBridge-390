# Matriz de testes

## Testes locais

| Comando | Cobertura |
|---|---|
| `make` | compila gateway, emulador e inspector |
| `make schema` | calcula campos, offsets e tamanho do copybook |
| `make test` | unidade, integração TCP, COBOL real e segurança |
| `make api-test` | HTTP JSON até o emulador C |
| `make load-test` | concorrência da API; exige serviços ativos |
| `make z390-test` | compila, linka e executa a regra COBOL com zCOBOL/z390 |
| `make asan-test` | executa C com AddressSanitizer e UndefinedBehaviorSanitizer |
| `make hardening-test` | testa TCP fragmentado, NaN, body grande e readiness |

O `test.sh` cobre sucesso, valor zero, moeda inválida, operação inválida,
JSON incompatível, backend indisponível, copybook incompatível e múltiplas
requisições sequenciais. O `cobol_api_test.sh` usa portas temporárias para
evitar conflito com processos já abertos.

O `api_test.sh` também verifica replay idempotente e conflito quando a mesma
`Idempotency-Key` aparece com outro payload. O `asan-test` deve ser executado
antes de alterações de baixo nível em C; ele pode exigir uma máquina com mais
memória que o teste normal.

## Testes de Hercules

```bash
make hercules-check
make hercules-mvs-check
make hercules-jcl-check
```

O primeiro valida apenas a configuração mínima. O segundo inicia o TK5 e
aguarda o system log/JES2. O terceiro envia `jcl/ironbridge-proof.jcl` ao
reader TCP `3505` e verifica `IEBGENER RC=0000` no hardcopy. O TK5 fica fora
do Git por tamanho e licença; o script informa se a imagem não estiver
instalada.

O teste `make z390-test` é separado do TK5: comprova a toolchain COBOL
mainframe e o runtime z390 com limites de CPU/memória, sem iniciar um guest
Hercules. Isso reduz o risco de travar a estação durante a validação.

## Evidências

As imagens em `docs/evidence/` são artefatos de demonstração, não substitutos
dos comandos reproduzíveis. Ao gerar novas evidências, capture a saída do
comando completo e registre data, versão do compilador e ambiente no Markdown.
