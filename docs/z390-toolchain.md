# Toolchain COBOL executável para MVS/Hercules

O limite mais importante do laboratório foi investigado. `cobc`/GnuCOBOL
traduz COBOL para C e gera um executável nativo do Linux; esse ELF não é um
load module MVS. Portanto, simplesmente copiar `cobol-backend` para um volume
do guest nunca poderia funcionar.

Existe uma alternativa pública para experimentar geração de código mainframe:
o projeto z390 inclui o zCOBOL, que traduz COBOL para fonte assembler HLASM
compatível e fornece um runtime/emulador. A própria documentação do zCOBOL
avisa que o compilador ainda está em estado beta e não implementa todo o
COBOL ANSI 1985. Isso o torna adequado para um caminho experimental de
compilação e prova, não para declarar compatibilidade total com IBM Enterprise
COBOL.

Fontes do projeto: [z390 no GitHub](https://github.com/z390development/z390),
[guia do zCOBOL](https://z390development.github.io/z390/user_guide/zCOBOL/zCOBOL_System_Programmer_Guide/).

## Instalação local

O download fica fora do código do projeto:

```bash
mkdir -p .tools
git clone https://github.com/z390development/z390.git .tools/z390
```

O Java já é requisito do z390. Verifique:

```bash
java -version
find .tools/z390 -maxdepth 2 -type f -name 'z390.jar' -o -name 'zc390.class'
```

O repositório inclui exemplos em `zcobol/demo`. Primeiro valide o compilador
com o exemplo oficial antes de tentar adaptar o backend do projeto:

```bash
cd .tools/z390
java -jar z390.jar zcobol/demo/HELLO.CBL
```

Os nomes exatos dos comandos variam com a versão do z390; o Makefile não deve
baixar automaticamente esse componente nem iniciar o Hercules sem limite de
recursos. Neste projeto, a fonte compatível com o parser beta fica em
`cobol/transaction_z390.cbl`; a fonte `transaction.cob` continua sendo a
versão completa usada pelo GnuCOBOL.

Compile e execute a prova zCOBOL a partir de uma pasta sem espaços no caminho
(o z390 V1.8.05 possui scripts antigos sem quoting completo):

```bash
ln -sfn "$PWD" /tmp/ironbridge-390
cd /tmp/ironbridge-390
timeout --foreground --kill-after=10s 120s \
  bash .tools/z390/bash/cblclg cobol/transaction_z390.cbl
```

Use o nome curto `cobol/trx390.cbl`, pois o linker histórico do z390 trabalha
com nomes de programa de até oito caracteres:

```bash
timeout --foreground --kill-after=10s 120s \
  bash .tools/z390/bash/cblclg cobol/trx390.cbl
```

O resultado esperado contém `ZC390 RC= 0`, `MZ390 RC= 0`, `LZ390 RC= 0` e a
saída `OK/000 Transacao validada pelo core legado`. Isso prova a compilação,
linkedição e execução pelo runtime z390. Ainda é uma execução no emulador
z390, não um job no JES2 do TK5; são duas camadas diferentes e ambas são
úteis para demonstrar o fluxo.

## Por que isso ainda não substitui o backend GnuCOBOL

O backend atual usa chamadas POSIX (`ib_socket_*`) para falar com o gateway.
Um load module MVS não pode chamar essa shim Linux. O caminho mainframe real
precisa escolher uma destas integrações:

1. programa batch com DD/SYSIN e SYSOUT;
2. sockets TCP via APIs TCP/IP disponíveis no guest;
3. transação CICS com `EXEC CICS RECEIVE/SEND`;
4. serviço IMS com a interface de mensagem correspondente.

O zCOBOL pode provar a geração de um programa COBOL simples no ecossistema
z390. Para transportar o IronBridge completo ainda seria necessário escrever
o adaptador de I/O MVS, montar/linkar o runtime e transferir o load module
para um dataset. Isso é trabalho de portabilidade, não uma troca de extensão
de arquivo.

## Segurança do teste

Os scripts `hercules-mvs-check` e `hercules-jcl-check` agora aplicam:

- limite de tempo externo;
- limite de CPU do processo filho;
- limite de memória virtual compatível com a reserva do Java;
- prioridade reduzida (`nice`);
- envio do comando `exit` ao console antes de encerrar;
- limpeza do grupo de processos.

Assim, um teste malformado não deve congelar a máquina nem exigir desligamento
forçado. Se o guest não responder, o comando termina com erro e preserva o
host.
