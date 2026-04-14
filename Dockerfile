FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
  build-essential \
  cmake \
  libssl-dev \
  openssl \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY server ./server

RUN cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLIENT=OFF \
    -DBUILD_TESTS=OFF
RUN cmake --build build --target server


RUN openssl req -x509 -newkey rsa:4096 \
    -keyout /app/key.pem \
    -out /app/cert.pem \
    -days 365 -nodes \
    -subj "/CN=monochat"


FROM ubuntu:24.04 AS runtime

RUN apt-get update && apt-get install -y \
  libssl3 \
  ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/server /app/server
COPY --from=builder /app/cert.pem     /app/cert.pem
COPY --from=builder /app/key.pem      /app/key.pem

EXPOSE 5555

ENTRYPOINT ["/app/server"]
CMD ["5555"]
