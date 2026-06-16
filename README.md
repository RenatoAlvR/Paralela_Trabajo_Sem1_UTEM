# Trabajo Paralela — Procesamiento Masivo de Transacciones

Este repositorio contiene una solución de alto rendimiento escrita en **C++** utilizando **OpenMP** y **libcurl**. Su propósito es procesar masivamente millones de transacciones de ventas:
1. **Descarga concurrente (SFTP)** de reportes transaccionales (archivos CSV).
2. **Parseo local seguro** para extraer montos de compra y UUIDs de clientes.
3. **Consumo masivo de API REST** (hasta 48 hilos concurrentes) para descubrir el género asociado a cada cliente, manejando tokens JWT en caliente.
4. **Cálculo paralelo** del gasto promedio histórico por género.

El sistema fue diseñado con resiliencia y tolerancia a fallos, mitigando los cuellos de botella de red (*I/O Bound*) mediante paralelismo de bucles y persistencia de conexiones (*HTTP Keep-Alive*).

---

## 🚀 Ejecución Rápida (Recomendado: Docker)

Para evitar problemas de compatibilidad en Windows o macOS, el proyecto incluye un entorno pre-configurado basado en Ubuntu.

1. **Construye la imagen (solo la primera vez):**
   ```bash
   docker build -t farmacia .
   ```
2. **Ejecuta el programa en modo paralelo (48 hilos):**
   ```bash
   # En Windows PowerShell, reemplaza $(pwd) por ${PWD}
   docker run --rm -v $(pwd):/app farmacia bash -c "./farmacia_parallel"
   ```
*(Nota: El comando asume que ya existe un binario compilado. Si deseas recompilar, añade `make &&` antes de ejecutar).*

---

## 🐧 Ejecución Nativa en Ubuntu 24.04 LTS

Si posees un entorno Linux nativo, el binario compilado (`farmacia_parallel`) ya está incluido en este repositorio listo para ser ejecutado.

### Requisitos previos (Solo si deseas recompilar el código fuente):
```bash
sudo apt-get update && sudo apt-get install -y build-essential libomp-dev libcurl4-openssl-dev libssh2-1-dev
```

### Comandos:
* **Ejecutar directamente:**
  ```bash
  ./farmacia_parallel
  ```
* **Forzar una ejecución secuencial (Benchmark):**
  ```bash
  ./farmacia_parallel --seq
  ```
* **Recompilar el proyecto:**
  ```bash
  make
  ```
* **Limpiar el proyecto (Preserva los CSV descargados en `data/`):**
  ```bash
  make clean
  ```

---

## 📄 Salidas del Sistema
Al finalizar la ejecución, el sistema generará:
* `resultados.txt`: Los montos promedio por género y el tiempo total.
* `log.txt`: Registro seguro de errores y reintentos capturados por los hilos.
