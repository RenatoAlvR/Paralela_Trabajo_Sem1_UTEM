#Esta es la versión de Linux que se va a usar
FROM ubuntu:24.04

#Previene terminales interactivas al ejecutar
ENV DEBIAN_FRONTEND=noninteractive

#Updatear e instalar librerias de red, compiladores, etc
RUN apt-get update && apt-get install -y \
    build-essential \
    libomp-dev \
    libcurl4-openssl-dev \
    libssh2-1-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

#Coloca al directorio de trabajo dentro del contenedor
WORKDIR /app