       >>SOURCE FORMAT FREE
       IDENTIFICATION DIVISION.
       PROGRAM-ID. IRONBRIDGE-COBOL-BACKEND.
       DATA DIVISION.
       WORKING-STORAGE SECTION.
       01 SERVER-SOCKET       PIC S9(9) COMP-5.
       01 CLIENT-SOCKET       PIC S9(9) COMP-5.
       01 PORT                PIC S9(9) COMP-5.
       01 SOCKET-RESULT       PIC S9(9) COMP-5.
       01 FRAME               PIC X(1024).
       01 REQUEST-ASCII       PIC X(42).
       01 RESPONSE-ASCII      PIC X(85).
       01 TRANSACTION-REQUEST.
          05 TRX-ID           PIC X(16).
          05 ACCOUNT-ID       PIC X(12).
          05 AMOUNT           PIC 9(9)V99 COMP-3.
          05 CURRENCY-CODE    PIC X(3).
          05 OPERATION        PIC X(5).
       01 TRANSACTION-RESPONSE.
          05 RESPONSE-STATUS  PIC X(2).
          05 RESPONSE-CODE    PIC X(3).
          05 RESPONSE-MESSAGE PIC X(64).
          05 RESPONSE-TRX-ID  PIC X(16).

       PROCEDURE DIVISION.
           ACCEPT PORT FROM ARGUMENT-VALUE
           IF PORT IS NOT NUMERIC
               MOVE 3901 TO PORT
           END-IF
           CALL "ib_socket_listen" USING BY VALUE PORT
               RETURNING SERVER-SOCKET
           IF SERVER-SOCKET < 0
               DISPLAY 'COBOL: falha ao abrir socket'
               STOP RUN
           END-IF
           DISPLAY 'COBOL backend ouvindo em ' PORT
           PERFORM ACCEPT-LOOP UNTIL 1 = 0
           GOBACK.

       ACCEPT-LOOP.
           CALL "ib_socket_accept" USING BY VALUE SERVER-SOCKET
               RETURNING CLIENT-SOCKET
           IF CLIENT-SOCKET >= 0
               CALL "ib_socket_recv_exact" USING BY VALUE CLIENT-SOCKET
                   BY REFERENCE FRAME BY VALUE 48 RETURNING SOCKET-RESULT
               IF SOCKET-RESULT = 48
                   CALL "ib_ebcdic_to_ascii" USING BY REFERENCE FRAME(7:42)
                       BY REFERENCE REQUEST-ASCII BY VALUE 42
                   MOVE REQUEST-ASCII TO TRANSACTION-REQUEST
                   PERFORM VALIDATE-TRANSACTION
                   MOVE SPACES TO RESPONSE-ASCII
                   MOVE TRANSACTION-RESPONSE TO RESPONSE-ASCII
                   CALL "ib_ascii_to_ebcdic" USING BY REFERENCE RESPONSE-ASCII
                       BY REFERENCE FRAME(7:85) BY VALUE 85
                   MOVE 'IB' TO FRAME(1:2)
                   MOVE X"01" TO FRAME(3:1)
                   MOVE X"02" TO FRAME(4:1)
                   MOVE X"00" TO FRAME(5:1)
                   MOVE X"55" TO FRAME(6:1)
                   CALL "ib_socket_send_exact" USING BY VALUE CLIENT-SOCKET
                       BY REFERENCE FRAME BY VALUE 91 RETURNING SOCKET-RESULT
               END-IF
               CALL "ib_socket_close" USING BY VALUE CLIENT-SOCKET
           END-IF
           EXIT.

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
               MOVE 'Transacao validada pelo core COBOL' TO RESPONSE-MESSAGE
           END-IF
           END-IF
           END-IF
           EXIT.
