# IronBridge-390 — arquitetura

## Fluxo executável

```mermaid
flowchart LR
    Client[Cliente REST JSON] --> API[api.py :8080]
    API -->|UTF-8 JSON + correlation ID| Gateway[Gateway C concorrente]
    Gateway -->|frame TCP| COBOL[Backend COBOL/GnuCOBOL]
    COBOL -->|EBCDIC + COMP-3| Gateway
    Gateway --> API
    API --> Client
```

## Contrato binário

```mermaid
sequenceDiagram
    participant A as API
    participant G as Gateway C
    participant C as COBOL
    A->>G: JSON UTF-8 + X-Correlation-ID
    G->>G: valida schema e calcula COMP-3
    G->>C: header IB + payload EBCDIC
    C->>C: move para copybook e valida transação
    C-->>G: resposta EBCDIC
    G-->>A: JSON + correlation header
```

## Produção versus laboratório

O laboratório usa `cobol_socket_shim.c` para disponibilizar sockets POSIX ao
GnuCOBOL. Em z/OS, esse ponto é substituído por CICS TCP/IP, `EZASMI` ou um
monitor de transação; o copybook e o contrato de 42 bytes permanecem iguais.
