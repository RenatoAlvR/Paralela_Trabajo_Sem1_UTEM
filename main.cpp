#include <cstdio>
#include <curl/curl.h>
#include <omp.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "utils.hpp"
#include "csv_parser.hpp"
#include "sftp_manager.hpp"
#include "api_manager.hpp"
#include "metrics_processor.hpp"

using namespace std;

int main(int argc, char **argv)
{
    double start_time;
    double end_time;
    double total_elapsed_time;
    vector<Transaction> all_transactions;
    set<string> unique_uuids_set;
    unordered_map<string, string> client_genders_cache;

    log_file = fopen("log.txt", "w");

    if (argc > 1 && string(argv[1]) == "--seq")
    {
        omp_set_num_threads(1);
    }

    start_time = omp_get_wtime();

    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Descarga y parseo de reportes CSV desde el servidor SFTP.
    download_sftp_files(all_transactions);
    printf("Transactions: %zu\n", all_transactions.size());

    // Extracción de UUIDs únicos desde las transacciones.
    for (const auto &current_transaction : all_transactions)
    {
        unique_uuids_set.insert(current_transaction.client_uuid);
    }
    printf("Unique UUIDs: %zu\n", unique_uuids_set.size());

    // Consulta concurrente de géneros mediante la API REST.
    query_client_api(unique_uuids_set, client_genders_cache);

    curl_global_cleanup();

    end_time = omp_get_wtime();
    total_elapsed_time = end_time - start_time;

    // Cálculo de métricas y escritura de resultados.
    calculate_and_save_metrics(all_transactions, client_genders_cache, total_elapsed_time);

    fclose(log_file);

    return 0;
}