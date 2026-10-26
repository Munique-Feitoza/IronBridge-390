       >>SOURCE FORMAT FREE
       IDENTIFICATION DIVISION.
       PROGRAM-ID. IRONBRIDGE-TRANSACTION.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       *> Modo atual: o programa é chamado pelo runner do ambiente mainframe.
       *> Para usar como serviço TCP em GnuCOBOL, remova os comentários do
       *> bloco abaixo e ligue as chamadas C$SOCKET ao framing do protocolo.
       *> Em z/OS, substitua C$SOCKET por EZASMI/TCPIP ou por uma transação CICS.
       *> O layout de TRANSACTION-REQUEST não muda: continua sendo o contrato
       *> binário de 42 bytes produzido pelo IronBridge.
       *> 
       *> 01 SOCKET-HANDLE     PIC S9(9) COMP-5.
       *> 01 SOCKET-ADDRESS    PIC X(16).
       *> 01 FRAME-BUFFER      PIC X(1024).
       *> 01 FRAME-LENGTH      PIC 9(4) COMP-5.
       *>     CALL "C$SOCKET" USING "server", SOCKET-HANDLE.
       *>     CALL "C$SOCKET" USING "bind", SOCKET-HANDLE, 3901.
       *>     CALL "C$SOCKET" USING "listen", SOCKET-HANDLE, 16.
       *> ACCEPT-LOOP.
       *>     CALL "C$SOCKET" USING "accept", SOCKET-HANDLE, SOCKET-HANDLE.
       *>     CALL "C$SOCKET" USING "recv", SOCKET-HANDLE,
       *>          FRAME-BUFFER, FRAME-LENGTH.
       *>     MOVE FRAME-BUFFER(7:42) TO TRANSACTION-REQUEST.
       *>     PERFORM VALIDATE-TRANSACTION.
       *>     MOVE TRANSACTION-RESPONSE TO FRAME-BUFFER(7:85).
       *>     CALL "C$SOCKET" USING "send", SOCKET-HANDLE,
       *>          FRAME-BUFFER, 91.
       *>     GO TO ACCEPT-LOOP.
       01 TRANSACTION-REQUEST.
          05 TRX-ID       PIC X(16).
          05 ACCOUNT-ID   PIC X(12).
          05 AMOUNT       PIC 9(9)V99 COMP-3.
          05 CURRENCY-CODE PIC X(3).
          05 OPERATION    PIC X(5).
       01 TRANSACTION-RESPONSE.
          05 RESPONSE-STATUS PIC X(2).
          05 RESPONSE-CODE   PIC X(3).
          05 RESPONSE-MESSAGE PIC X(64).
          05 RESPONSE-TRX-ID PIC X(16).
       PROCEDURE DIVISION.
           *> Smoke test local. Ao ativar o bloco TCP, substitua este início
           *> pelo ACCEPT-LOOP comentado acima.
           MOVE 'TRX000000000001' TO TRX-ID
           MOVE '123456789012' TO ACCOUNT-ID
           MOVE 125.37 TO AMOUNT
           MOVE 'BRL' TO CURRENCY-CODE
           MOVE 'DEBIT' TO OPERATION
           PERFORM VALIDATE-TRANSACTION
           DISPLAY RESPONSE-STATUS '/' RESPONSE-CODE ' ' RESPONSE-MESSAGE
           GOBACK.

       VALIDATE-TRANSACTION.
           MOVE TRX-ID TO RESPONSE-TRX-ID
           IF AMOUNT IS NOT GREATER THAN ZERO
               MOVE 'ER' TO RESPONSE-STATUS
               MOVE '422' TO RESPONSE-CODE
               MOVE 'Valor deve ser maior que zero' TO RESPONSE-MESSAGE
           ELSE IF CURRENCY-CODE NOT = 'BRL'
               MOVE 'ER' TO RESPONSE-STATUS
               MOVE '422' TO RESPONSE-CODE
               MOVE 'Moeda nao suportada' TO RESPONSE-MESSAGE
           ELSE IF OPERATION NOT = 'DEBIT' AND OPERATION NOT = 'CRED'
               MOVE 'ER' TO RESPONSE-STATUS
               MOVE '422' TO RESPONSE-CODE
               MOVE 'Operacao invalida' TO RESPONSE-MESSAGE
           ELSE
               MOVE 'OK' TO RESPONSE-STATUS
               MOVE '000' TO RESPONSE-CODE
               MOVE 'Transacao validada pelo core legado' TO RESPONSE-MESSAGE
           END-IF
           END-IF
           END-IF
           EXIT.
