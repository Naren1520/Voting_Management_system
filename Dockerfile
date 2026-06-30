# Build stage
FROM gcc:12 AS builder

WORKDIR /app

# Copy source files
COPY backend/voting_server_linux.cpp .
COPY backend/json.hpp .

# Compile the C++ server
RUN g++ -std=c++17 -O2 -o voting_server voting_server_linux.cpp -pthread

# Runtime stage (smaller image)
FROM debian:bookworm-slim

WORKDIR /app

# Copy compiled binary from builder
COPY --from=builder /app/voting_server .

# Copy initial data files
COPY backend/data/ ./data/

# Railway uses PORT env variable
EXPOSE 8080

CMD ["./voting_server"]
