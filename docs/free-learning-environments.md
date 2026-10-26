# Ambientes gratuitos e legais para aprendizado IBM Z

Este projeto pode ser demonstrado sem afirmar que uma imagem IBM licenciada é
gratuita. Existem três caminhos legais, com objetivos diferentes.

## 1. IBM Z Xplore — melhor primeiro passo

[IBM Z Xplore](https://www.ibm.com/products/z/resources/zxplore) é uma
plataforma de aprendizado sem custo, disponível globalmente. Ela oferece
desafios práticos de JCL, COBOL, datasets, USS, VSAM, Db2 e segurança, além de
badges digitais.

### Como começar

1. Abra a página do IBM Z Xplore.
2. Crie ou use um IBMid.
3. Entre no percurso Fundamentals + Concepts.
4. Avance para Advanced, COBOL, JCL, VSAM e Db2.
5. Guarde os badges e as telas de execução como evidência de aprendizado.

Esse ambiente é excelente para aprender e mostrar conhecimento, mas pode não
permitir instalar o IronBridge inteiro ou expor uma porta TCP própria. Nesse
caso, use o projeto local para a API e o Xplore para provar conhecimento de
JCL/COBOL/IBM Z.

## 2. IBM Z Trial — prova rápida de z/OS

O [IBM Z Software Trials](https://www.ibm.com/products/z/trials) oferece
ambientes IBM Z pré-configurados sem instalação. O catálogo informa trials de
três dias, normalmente liberados após registro, além de trials específicos de
outros produtos.

Existe também o [IBM Z Development and Test Environment IBM Z
Trial](https://early-access.ibm.com/software/support/trial/cst/welcomepage.wss?siteId=1179&tabId=3094&w=1), descrito pela IBM como um ambiente
gratuito, sem cartão e sem instalação, para explorar uma distribuição z/OS
pré-construída.

### O que fazer nesse trial

- validar login e navegação em TSO/ISPF;
- criar dataset de origem;
- submeter JCL;
- consultar spool e códigos de retorno;
- executar um programa ou utilitário disponibilizado pelo ambiente;
- capturar telas com data, job name e `RC=0000`.

O trial tem duração e escopo definidos pelo fornecedor. Não coloque dados
reais, segredos ou workloads de produção nele.

## 3. CICS Transaction Server Developer Trial

O [CICS Transaction Server for z/OS Developer
Trial](https://www.ibm.com/products/cics-transaction-server) é uma edição de
avaliação sem custo para desenvolvimento e teste. A documentação informa
validade de pelo menos 90 dias a partir do pedido, restrições de desempenho e
uso não produtivo.

### O que ele permite provar

- definição de uma transação CICS;
- programa COBOL com `EXEC CICS RECEIVE` e `EXEC CICS SEND`;
- definição de programa e transação;
- segurança e autorização;
- integração entre o gateway e uma transação CICS;
- retorno de `COMMAREA` ou canal/container.

O Developer Trial precisa ser instalado em um z/OS compatível. Ele não é um
pacote que se instala diretamente no MVS TK5.

## 4. IBM Z Development and Test Environment com autorização

O [ZD&T](https://www.ibm.com/docs/en/zdt/14.2.x?topic=overview) executa uma
distribuição z/OS em Linux/x86 para desenvolvimento e teste, mas não é uma
licença de produção. Dependendo da edição e do programa de acesso, pode exigir
licença, USB ou vínculo institucional.

Esse é o ambiente adequado para a prova completa de:

```text
z/OS -> COBOL -> CICS/IMS -> Db2/VSAM -> JCL -> IronBridge
```

## 5. O que não fazer

Não baixar imagens de z/OS, compiladores IBM, CICS, IMS ou Db2 de torrents,
cracks ou repositórios não autorizados. Além de ilegal, isso pode introduzir
malware e produzir resultados que não podem ser reproduzidos.

## Matriz de comprovação

| Objetivo | Local | Custo esperado | Evidência |
|---|---|---:|---|
| API, C, EBCDIC, COMP-3 | IronBridge local | gratuito | `make test` |
| COBOL compilado | GnuCOBOL + z390 | gratuito | `make z390-test` |
| IPL, JES2 e JCL | Hercules/TK5 | gratuito | `make hercules-jcl-check` |
| JCL em z/OS | IBM Z Trial/Xplore | trial/gratuito | job + spool |
| CICS real | CICS Developer Trial | avaliação | transação + CEDF/log |
| IMS real | ZD&T/ambiente IBM | depende do acesso | mensagem IMS + log |
| Db2/VSAM real | Xplore/ZD&T | depende do acesso | query/dataset + RC |

Registre exatamente o ambiente utilizado: integração simulada com
Hercules/MVS 3.8J, prova de toolchain zCOBOL/z390 ou execução em um trial IBM.
Mantenha associadas as evidências do job, da transação e da versão realmente
testados.
