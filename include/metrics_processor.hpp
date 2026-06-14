#ifndef METRICS_PROCESSOR_HPP
#define METRICS_PROCESSOR_HPP

// Procesamiento de métricas y cálculo estadístico paralelo del promedio de compras por género mediante reducciones de OpenMP.

#include <vector>
#include <unordered_map>
#include <string>
#include "csv_parser.hpp"

using namespace std;

// Procesa las transacciones de forma paralela para calcular las métricas de compras por género y el tiempo de ejecución.
void calculate_and_save_metrics(const vector<Transaction> &transactions_list, const unordered_map<string, string> &client_genders_cache, double execution_time_seconds);

#endif // METRICS_PROCESSOR_HPP
