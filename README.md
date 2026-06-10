# Trabajo Paralelo — Un Problema de Cuadratura

## Requisitos

- Ubuntu 24.04 LTS
- GCC (g++) con soporte C++17
- OpenMP (libomp-dev)
- libcurl (libcurl4-openssl-dev)
- libssh2 (libssh2-1-dev)

## Instalacion de dependencias

```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential libomp-dev libcurl4-openssl-dev libssh2-1-dev ca-certificates
```

## Compilacion

```bash
make
```

## Ejecucion

Modo paralelo (por defecto):

```bash
./farmacia_parallel
```

Modo secuencial (para comparacion de rendimiento):

```bash
./farmacia_parallel --seq
```

## Docker

```bash
docker build -t farmacia .
docker run -v $(pwd):/app farmacia bash -c "make && ./farmacia_parallel"
```

## Archivos de salida

- `resultados.txt` — Promedios de MONTO APLICADO por genero y tiempo de ejecucion
- `log.txt` — Registro de errores durante la ejecucion

## Limpieza

```bash
make clean
```
