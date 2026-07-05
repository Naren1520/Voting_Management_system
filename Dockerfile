# ============================================================
# Dockerfile — VoteStack backend (Render.com deployment)
# Multi-stage build: full Debian builder -> debian-slim runtime
# ============================================================

# ── Build stage ───────────────────────────────────────────────
FROM debian:bookworm AS builder

WORKDIR /app

RUN apt-get update && \
    apt-get install -y \
        g++ \
        make \
        libcurl4-openssl-dev \
        libssl-dev \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

COPY backend/server/include/     ./include/
COPY backend/server/src/         ./src/
COPY backend/server/third_party/ ./third_party/

RUN mkdir -p bin && \
    g++ -std=c++17 -O2 -Wall -pthread \
        src/main.cpp \
        src/core/Config.cpp \
        src/core/Logger.cpp \
        src/core/ThreadPool.cpp \
        src/net/EpollServer.cpp \
        src/net/HttpRequest.cpp \
        src/net/HttpResponse.cpp \
        src/db/SupabaseClient.cpp \
        src/cache/RedisClient.cpp \
        src/cache/CandidateCache.cpp \
        src/controllers/AuthController.cpp \
        src/controllers/ElectionController.cpp \
        src/controllers/CandidateController.cpp \
        src/controllers/VoterController.cpp \
        src/controllers/PositionController.cpp \
        src/controllers/PublicVoteController.cpp \
        src/controllers/PublicMultiVoteController.cpp \
        -Iinclude -Ithird_party \
        -lcurl -lssl -lcrypto \
        -o bin/voting_server

# ── Runtime stage ─────────────────────────────────────────────
FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y \
        libcurl4 \
        libssl3 \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/bin/voting_server .

# Render injects PORT automatically — the server reads it from env.
# Default is 8080 if PORT is not set.
EXPOSE 8080

CMD ["./voting_server"]
