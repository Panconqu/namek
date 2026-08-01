FROM rust:1-slim-bookworm AS builder
WORKDIR /app
COPY backend/Cargo.toml backend/Cargo.lock ./
RUN mkdir src && echo 'fn main() {}' > src/main.rs \
    && cargo build --release || true
COPY backend/src ./src
RUN cargo build --release

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/target/release/namek_backend /usr/local/bin/namek_backend
ENV BACKEND_PORT=8080
EXPOSE 8080
CMD ["namek_backend"]
