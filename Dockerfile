# Build stage
FROM gcc:12 AS builder

WORKDIR /app

# Copy source files
COPY backend/voting_server_linux.cpp .
COPY backend/json.hpp .

# Compile the C++ server
RUN g++ -std=c++17 -O2 -o voting_server voting_server_linux.cpp -pthread

# Runtime stage
FROM debian:bookworm-slim

# Install curl (needed to call Supabase REST API)
RUN apt-get update && apt-get install -y curl && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiled binary
COPY --from=builder /app/voting_server .

# Railway/Render uses PORT env variable
EXPOSE 8080

CMD ["./voting_server"]
