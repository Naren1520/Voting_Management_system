# Build stage
FROM gcc:12 AS builder

WORKDIR /app

COPY backend/voting_server_linux.cpp .
COPY backend/json.hpp .

RUN g++ -std=c++17 -O2 -o voting_server voting_server_linux.cpp -pthread

# Runtime stage
FROM debian:bookworm-slim

# curl for Supabase REST API, openssl for password hashing + token generation
RUN apt-get update && \
    apt-get install -y curl openssl && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/voting_server .

EXPOSE 8080

CMD ["./voting_server"]
