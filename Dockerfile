FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends build-essential gnucobol python3 curl ca-certificates netcat-openbsd && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN make all cobol-backend
RUN useradd --create-home --uid 10001 ironbridge && mkdir -p /var/lib/ironbridge && chown -R ironbridge:ironbridge /app /var/lib/ironbridge
USER ironbridge
EXPOSE 3900 3901 8080
HEALTHCHECK --interval=10s --timeout=3s --start-period=10s --retries=3 CMD curl -fsS http://127.0.0.1:8080/health || exit 1
