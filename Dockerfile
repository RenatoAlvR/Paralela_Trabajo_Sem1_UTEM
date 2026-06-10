FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    libomp-dev \
    libcurl4-openssl-dev \
    libssh2-1-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app