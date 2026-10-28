# IronBridge-390

Licença: [MIT](LICENSE).

**Versão: 1.0.0** — primeira versão demonstrável.

Consulte as [notas de lançamento da v1.0.0](docs/release-notes-v1.0.0.md)
para ver o escopo, as validações e as limitações conhecidas desta versão.

Middleware demonstrativo para integrar uma API moderna em JSON/UTF-8 com um
backend orientado a copybook COBOL/EBCDIC. O projeto usa um protocolo binário
framed, campos de tamanho fixo, `COMP-3` para valores monetários e uma ponte
TCP entre o mundo moderno e o legado.

Documentação visual: [arquitetura](docs/architecture.md), [operação](docs/operations.md)
e [adaptação CICS/IMS/Hercules](docs/mainframe-adapter.md).

Para entender o projeto em profundidade, consulte o [guia técnico](docs/project-guide.md),
o [contrato do protocolo](docs/protocol.md) e a [matriz de testes](docs/testing.md).

O perfil local do Hercules está em [hercules/README.md](hercules/README.md).
As provas visuais do IPL e da execução de JCL estão em
[docs/evidence/mvs-execution.md](docs/evidence/mvs-execution.md).

## Fluxo

```text
cliente JSON/UTF-8 -> ironbridge (3900) -> frame binário/EBCDIC -> backend (3901)
                         <- JSON/UTF-8 <- frame binário/EBCDIC <-
```

O backend local em C (`backend-emulator`) tem a mesma lógica e o mesmo layout
do programa COBOL, permitindo reproduzir os testes sem instalar Hercules ou
GnuCOBOL. Em um ambiente mainframe, ele é substituído por `cobol/transaction.cob`.
O fonte COBOL inclui, em comentários, o adaptador TCP para ativação no
GnuCOBOL; basta remover o bloco e ajustar a biblioteca de sockets disponível.

### Verificação no USS do IBM Z XPlore

O USS foi usado como uma segunda estação de teste. Ele possui IBM Semeru Java
17, `javac`, XL C/C99 e `make`; o cliente Java, o backend C e o gateway C foram
compilados nesse ambiente com as opções z/OS (`-qascii`, sockets e pthreads).
O teste ponta a ponta Java → gateway C → backend C também foi executado com
sucesso no USS. O gateway inclui um parser IPv4 compatível com os argumentos
ASCII/EBCDIC do runtime z/OS. Atualmente esse caminho aceita endereços IPv4
numéricos; resolução de nomes DNS no próprio gateway ainda pode ser adicionada
se for necessária.

## Copybook da transação

```cobol
01 TRANSACTION-REQUEST.
   05 TRX-ID       PIC X(16).       *> EBCDIC, identificador
   05 ACCOUNT-ID   PIC X(12).       *> EBCDIC, conta
   05 AMOUNT       PIC 9(9)V99 COMP-3. *> 11 dígitos, 6 bytes
   05 CURRENCY-CODE PIC X(3).       *> EBCDIC, ex.: BRL
   05 OPERATION    PIC X(5).        *> EBCDIC, ex.: DEBIT
```

O payload tem 42 bytes: `16 + 12 + 6 + 3 + 5`. O valor monetário é enviado em
centavos como 11 dígitos decimais packed decimal; o último nibble é `C` para
positivo e `D` para negativo. O header de rede tem 6 bytes:

```text
offset  tamanho  campo
0       2        magic ASCII: IB
2       1        versão: 1
3       1        tipo: 1=request, 2=response
4       2        payload length, uint16 big-endian
```

## Executar

```bash
make
./backend-emulator 3901
./ironbridge 3900 127.0.0.1 3901 copybook.cpy
python3 client.py --port 3900 --transaction-id TRX000000000001 \
  --account-id 123456789012 --amount 125.37 --currency BRL --operation DEBIT
```

O mesmo gateway também pode ser usado por um cliente Java, sem dependências
externas:

```bash
make java
java -cp java IronBridgeClient 127.0.0.1 3900 TRXJAVA00000001 \
  123456789012 125.37 BRL DEBIT
```

Assim, Python e Java exercitam exatamente o mesmo contrato de entrada e a
mesma ponte para o backend COBOL/legado.

Para expor uma API REST real, mantenha o backend e o gateway ativos e inicie
o adaptador HTTP:

```bash
python3 api.py --port 8080 --gateway-port 3900
curl -X POST http://127.0.0.1:8080/transactions \
  -H 'Content-Type: application/json' \
  -d '{"transaction_id":"TRX000000000001","account_id":"123456789012","amount":"125.37","currency":"BRL","operation":"DEBIT"}'
```

Teste essa camada com:

```bash
make api-test
```

Para testar o caminho completo com o programa COBOL real:

```bash
make cobol-backend
make cobol-api-test
```

Esse teste comprova `REST HTTP -> gateway C -> frame EBCDIC/COMP-3 -> backend COBOL -> JSON HTTP`.

Para executar manualmente o backend COBOL em uma porta específica:

```bash
./cobol-backend 3901
```

Para testar automaticamente:

```bash
make test
```

O alvo `make test` também compila e testa o cliente Java contra o gateway e o
backend locais.

Testes adicionais de robustez:

```bash
make hardening-test
make asan-test
```

`hardening-test` verifica TCP fragmentado, payload numérico inválido, body
HTTP excedente, `/ready` e copybook com nomes alternativos. `asan-test` roda o
código C com AddressSanitizer e UndefinedBehaviorSanitizer.

Para provar compilação, linkedição e execução da regra COBOL com zCOBOL/z390:

```bash
make z390-test
```

Esse alvo é isolado do Hercules, possui limites de recursos e está explicado
em [docs/z390-toolchain.md](docs/z390-toolchain.md).

A evidência da execução está em
[docs/evidence/z390-execution.md](docs/evidence/z390-execution.md).

As opções legais de aprendizado e avaliação IBM Z estão em
[docs/free-learning-environments.md](docs/free-learning-environments.md).

O CI executa esse mesmo comando em cada push e pull request. Para teste de
carga, mantenha a API ativa e rode:

```bash
make load-test
```

O gateway usa workers `pthread`, timeouts e limite de concorrência, registra
eventos JSON com correlation ID e a API oferece `/health` e `/metrics`.
Bearer token, TLS e idempotência SQLite são opcionais para o ambiente local e
podem ser ativados conforme [operations.md](docs/operations.md).

O build não depende de bibliotecas externas. `cobc` é opcional; se instalado,
`make cobol` compila o programa COBOL.

### COBOL no VS Code

O repositório já associa `.cob` e `.cpy` à linguagem COBOL e `.jcl` à linguagem
JCL em `.vscode/settings.json`. Para syntax highlighting e o ícone da linguagem,
instale a extensão recomendada `bitlang.cobol` pelo painel de extensões. O ícone
é fornecido pelo tema/extensão de ícones do VS Code, não pelo compilador
`cobc`; se o tema atual não mostrar o desenho COBOL, instale também um tema de
ícones que tenha esse mapeamento e recarregue a janela.

## Decisões técnicas principais

- A fronteira TCP usa framing explícito e ordem de bytes de rede, então uma
  leitura parcial não é confundida com uma mensagem completa.
- Texto de negócio é normalizado em UTF-8 no ingresso, validado como ASCII
  estrito e convertido para EBCDIC Code Page 037 no buffer legado.
- `COMP-3` armazena dois dígitos por byte e usa o nibble final como sinal; por
  isso dinheiro não passa por `float`.
- O parser usa offsets calculados a partir do copybook e valida capacidade,
  caracteres, tamanho e sinal antes de produzir o buffer.
- O emulador C é apenas um substituto local. A regra de negócio está duplicada
  no COBOL para demonstrar o contrato que seria executado em z/OS/CICS.

## Parser de copybook

O layout agora também pode ser carregado de um arquivo externo. O parser
suporta `PIC X(n)`, `PIC 9(n)`, casas decimais com `V` e `COMP-3`:

```bash
make schema
./copybook-inspect copybook.cpy
```

Ele mostra offset, tamanho físico, quantidade de dígitos e escala. A validação
do layout evita que o bridge seja configurado com um copybook incompatível.
O quinto argumento de `ironbridge` ativa essa validação no startup.

## Limites técnicos conhecidos

O parser já lê um copybook externo para o subconjunto de layout usado aqui:
`PIC X`, `PIC 9`, `V`, `S` e `COMP-3`, além de comentários e níveis COBOL. O
gateway ainda exige que o resultado tenha os 42 bytes do contrato e que os
cinco nomes semânticos sejam os campos desta transação. Isso é intencional:
um copybook arbitrário exige também um mapeamento entre nomes COBOL e campos
JSON, não apenas o cálculo de offsets.

`REDEFINES`, `OCCURS`, grupos expandidos, sinais separados e demais formatos
computacionais continuam fora do subconjunto seguro. A limitação de executar
o backend dentro do MVS 3.8J também está explicada, com evidência, em
[docs/evidence/mvs-execution.md](docs/evidence/mvs-execution.md): o job JCL
roda no guest, enquanto o COBOL de negócio roda com GnuCOBOL no host por falta
de uma toolchain COBOL compatível disponível na imagem.
