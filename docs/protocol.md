# Contrato de protocolo

## Header

Cada mensagem TCP começa com seis bytes:

| Offset | Bytes | Significado |
|---:|---:|---|
| 0 | 2 | `IB` em ASCII |
| 2 | 1 | versão `1` |
| 3 | 1 | tipo: `1` request, `2` response |
| 4 | 2 | tamanho do payload, inteiro big-endian |

O leitor usa `recv` repetidamente até completar header e payload. Isso é
necessário porque TCP entrega um fluxo: uma chamada de leitura pode retornar
apenas parte da mensagem.

O gateway aplica timeout de três segundos no cliente e no backend, limita o
corpo JSON HTTP a 8 KiB e limita o número de requisições simultâneas. Assim,
uma conexão lenta ou um backend travado não deve consumir indefinidamente os
recursos do processo.

## Request

O payload request tem 42 bytes. Campos textuais são preenchidos à direita com
espaços EBCDIC CP037. `AMOUNT` ocupa seis bytes de packed decimal, com 11
dígitos e duas casas decimais.

## Response

O payload response tem 85 bytes:

```text
STATUS  2 bytes  PIC X(2)
CODE    3 bytes  PIC X(3)
MESSAGE 64 bytes PIC X(64)
TRX-ID  16 bytes PIC X(16)
```

## Regras de negócio atuais

| Condição | Status | Código |
|---|---|---|
| valor maior que zero, BRL e `DEBIT`/`CRED` | `OK` | `000` |
| valor zero ou negativo | `ER` | `422` |
| moeda diferente de BRL | `ER` | `422` |
| operação diferente de DEBIT/CRED | `ER` | `422` |

As regras estão implementadas no backend COBOL e replicadas no emulador C
para que o laboratório continue testável sem GnuCOBOL.
