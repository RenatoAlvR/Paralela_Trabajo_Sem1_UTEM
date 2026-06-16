# Registro de Pruebas de Rendimiento

Este documento registra los resultados oficiales de las pruebas de rendimiento comparativas (Secuencial vs. Paralela) realizadas sobre el sistema de procesamiento de transacciones.

---

## 1. Configuración del Dataset de Prueba
Para realizar una comparación controlada y representativa en un tiempo razonable, se seleccionó un conjunto de **17 archivos CSV** de tamaño promedio.

* **Cantidad de archivos:** 17 archivos CSV.
* **Rango de reportes:** Desde `reporte_24102.csv` hasta `reporte_25001.csv`.
* **Total de transacciones procesadas:** 11.251 transacciones.
* **Total de clientes únicos (peticiones HTTP a la API REST):** 9.929 UUIDs.

---

## 2. Resultados: Ejecución Secuencial (1 Hilo)
* **Comando de ejecución:** `./farmacia_parallel --seq` (con el contador de hilos forzado a 1).
* **Tiempo de ejecución total:** **1.632,44 segundos** (27 minutos y 12 segundos).
* **Rendimiento promedio de red:** **6,08 consultas por segundo** (~164,4 ms por petición HTTP).
* **Valores de compra promedio calculados:**
  * **FEMENINO:** $8.176,095 CLP
  * **MASCULINO:** $8.934,690 CLP
* **Manejo de Errores y Expiración:**
  * Durante la ejecución, alrededor de la consulta **6.300 / 9.929**, el token JWT de la API REST expiró de forma natural.
  * El sistema detectó correctamente el código de error `HTTP 401 Unauthorized`.
  * La renovación automática del token se ejecutó exitosamente en caliente, y la consulta fallida se reintentó de inmediato con el nuevo token, completando el proceso con **0 fallos (100% de éxito)**.

---

## 3. Resultados: Ejecución Paralela (48 Hilos)
* **Comando de ejecución:** `./farmacia_parallel`
* **Tiempo de ejecución total:** **36,68 segundos**.
* **Rendimiento promedio de red:** **270,71 consultas por segundo** (~3,69 ms promedio por petición debido a concurrencia).
* **Valores de compra promedio calculados:**
  * **FEMENINO:** $8.176,095 CLP
  * **MASCULINO:** $8.934,690 CLP
* **Manejo de Concurrencia:**
  * Al inicializar manejadores CURL independientes por cada uno de los 48 hilos (HTTP Keep-Alive), se minimizó el overhead del protocolo TCP/TLS en las 9.929 consultas.
  * Los hilos accedieron y acumularon de manera segura los totales en paralelo mediante reducción paralela de OpenMP (`omp parallel for reduction`).

---

## 4. Comparativa de Rendimiento y Speedup
| Métrica | Ejecución Secuencial (1 Hilo) | Ejecución Paralela (48 Hilos) | Speedup ($S = T_{seq} / T_{par}$) |
|---|---|---|---|
| Tiempo total | 1.632,44 s | 36,68 s | **44,50x** |
| Rendimiento de Red | 6,08 req/s | 270,71 req/s | **44,52x** |
| Promedio FEMENINO | $8.176,095 | $8.176,095 | - |
| Promedio MASCULINO | $8.934,690 | $8.934,690 | - |

* **Eficiencia Paralela ($E = S / p$):** Con $p = 48$ hilos, la eficiencia es del **92,7%**, demostrando un aprovechamiento óptimo de los recursos concurrentes sin cuellos de botella por cerrojos o contención en red.
