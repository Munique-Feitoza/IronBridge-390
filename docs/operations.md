# Operação local

## Docker

```bash
docker compose up --build
```

O Compose executa os serviços como usuário não-root, aguarda healthchecks em
cascata e mantém o SQLite de idempotência no volume `ironbridge-data`.

No Manjaro, instale Docker e Compose com:

```bash
sudo pacman -Syu docker docker-compose
sudo systemctl enable --now docker.service
sudo usermod -aG docker "$USER"
```

Depois abra um novo login/terminal e valide:

```bash
docker --version
docker compose version
docker compose up --build
```

## Segurança

Ative Bearer token:

```bash
IRONBRIDGE_API_TOKEN=dev-secret python3 api.py --port 8080
curl -H 'Authorization: Bearer dev-secret' ...
```

Ative TLS fornecendo certificado e chave:

```bash
python3 api.py --tls-cert cert.pem --tls-key key.pem
```

## Métricas e carga

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/ready
curl http://127.0.0.1:8080/metrics
python3 load_test.py --requests 100 --workers 10
```

O gateway grava eventos JSON em stderr, incluindo `correlation_id`,
`transaction_id` e status.

## Idempotência

Para evitar reprocessar uma mesma requisição após retry do cliente, informe uma
chave e habilite o SQLite:

```bash
python3 api.py --port 8080 --db ./var/ironbridge.db
curl -X POST http://127.0.0.1:8080/transactions \
  -H 'Content-Type: application/json' \
  -H 'Idempotency-Key: payment-001' \
  -d '{"transaction_id":"TRX000000000001","account_id":"123456789012","amount":"125.37","currency":"BRL","operation":"DEBIT"}'
```

A mesma chave com o mesmo payload devolve a resposta armazenada. A mesma chave
com payload diferente é rejeitada. Em produção, o arquivo deve estar em um
volume persistente e com backup; para múltiplas réplicas, substitua SQLite por
um armazenamento transacional compartilhado.

Para um copybook que usa nomes diferentes, informe o arquivo `.map` como sexto
argumento do gateway. Um exemplo funcional está em
`copybook-alias.cpy`/`copybook-alias.map`.
