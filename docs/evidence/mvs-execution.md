# Evidências de execução

## IPL do MVS 3.8J

![Boot do MVS 3.8J e inicialização do JES2](mvs-ipl-proof.png)

O guest iniciou o IPL, ativou o system log e colocou o JES2 em warm-start.

## Job JCL executado dentro do MVS

![Job IBP002 executado pelo JES2](mvs-jcl-proof.png)

O job foi enviado pelo reader TCP `3505`, executado pelo initiator de classe A
e finalizado com `IEBGENER RC=0000`.

## Suíte do projeto

![Testes do gateway, COBOL, segurança e integração](full-test-proof.png)

```mermaid
sequenceDiagram
    participant Host as Host Linux
    participant H as Hercules/TK5
    participant JES as MVS/JES2
    Host->>H: inicia ./mvs
    H->>JES: IPL volume TK5RES
    Host->>H: envia jcl/ironbridge-proof.jcl:3505
    H->>JES: reader 1 recebe IBP002
    JES->>JES: INIT 1 / CLASS A
    JES-->>Host: IEBGENER RC=0000 / JOB ENDED
```

## Limite importante

Esta prova executa um job JCL dentro do MVS 3.8J. O backend COBOL de negócio
do IronBridge continua sendo executado via GnuCOBOL no host; para executá-lo
dentro do guest seria necessário compilá-lo com um compilador COBOL disponível
no MVS e transferir o load module para um dataset do guest.
