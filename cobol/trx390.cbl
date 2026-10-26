       IDENTIFICATION DIVISION.
       PROGRAM-ID. TRX390.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 TRANSACTION-REQUEST.
          05 TRX-ID           PIC X(16).
          05 ACCOUNT-ID       PIC X(12).
          05 AMOUNT           PIC 9(9)V99 COMP-3.
          05 CURRENCY-CODE    PIC X(3).
          05 OPERATION        PIC X(5).
       01 TRANSACTION-RESPONSE.
          05 ST               PIC X(2).
          05 CD               PIC X(3).
          05 MSG              PIC X(64).
          05 RID              PIC X(16).
       PROCEDURE DIVISION.
           MOVE 'TRX000000000001' TO TRX-ID
           MOVE '123456789012' TO ACCOUNT-ID
           MOVE 125.37 TO AMOUNT
           MOVE 'BRL' TO CURRENCY-CODE
           MOVE 'DEBIT' TO OPERATION
           PERFORM VALIDATE-TRANSACTION
           DISPLAY ST '/' CD ' ' MSG
           GOBACK.

       VALIDATE-TRANSACTION.
           MOVE TRX-ID TO RID
           IF AMOUNT IS NOT GREATER THAN ZERO
               MOVE 'ER' TO ST
               MOVE '422' TO CD
               MOVE 'Valor deve ser maior que zero' TO MSG
           ELSE
               IF CURRENCY-CODE NOT = 'BRL'
                   MOVE 'ER' TO ST
                   MOVE '422' TO CD
                   MOVE 'Moeda nao suportada' TO MSG
               ELSE
                   IF OPERATION NOT = 'DEBIT' AND OPERATION NOT = 'CRED'
                       MOVE 'ER' TO ST
                       MOVE '422' TO CD
                       MOVE 'Operacao invalida' TO MSG
                   ELSE
                       MOVE 'OK' TO ST
                       MOVE '000' TO CD
                       MOVE 'Transacao validada pelo core legado' TO MSG
                   END-IF
               END-IF
           END-IF
           EXIT.
