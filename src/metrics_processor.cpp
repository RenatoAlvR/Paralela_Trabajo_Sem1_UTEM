#include "metrics_processor.hpp"
#include "utils.hpp"
#include <cstdio>
#include <omp.h>
#include <vector>
#include <unordered_map>
#include <string>

using namespace std;

// Procesa las transacciones de forma paralela para calcular las métricas de compras por género y el tiempo de ejecución.
void calculate_and_save_metrics(const vector<Transaction> &transactions_list, const unordered_map<string, string> &client_genders_cache, double execution_time_seconds)
{
    using ClientGendersCacheIterator = unordered_map<string, string>::const_iterator;

    double female_total_amount;
    double male_total_amount;
    long female_transaction_count;
    long male_transaction_count;
    double female_average_purchase;
    double male_average_purchase;
    FILE *results_output_file;
    size_t transaction_index;

    female_total_amount = 0.0;
    male_total_amount = 0.0;
    female_transaction_count = 0;
    male_transaction_count = 0;

    #pragma omp parallel for reduction(+:female_total_amount, male_total_amount, female_transaction_count, male_transaction_count)
    for (transaction_index = 0; transaction_index < transactions_list.size(); ++transaction_index)
    {
        ClientGendersCacheIterator gender_cache_iterator;

        gender_cache_iterator = client_genders_cache.find(transactions_list[transaction_index].client_uuid);
        if (gender_cache_iterator != client_genders_cache.end())
        {
            if (gender_cache_iterator->second == "FEMENINO")
            {
                female_total_amount += transactions_list[transaction_index].purchase_amount_clp;
                female_transaction_count++;
            }
            else if (gender_cache_iterator->second == "MASCULINO")
            {
                male_total_amount += transactions_list[transaction_index].purchase_amount_clp;
                male_transaction_count++;
            }
        }
    }

    female_average_purchase = (female_transaction_count > 0) ? (female_total_amount / female_transaction_count) : 0.0;
    male_average_purchase = (male_transaction_count > 0) ? (male_total_amount / male_transaction_count) : 0.0;

    printf("FEMENINO = %f\n", female_average_purchase);
    printf("MASCULINO = %f\n", male_average_purchase);
    printf("TIEMPO = %f segundos\n", execution_time_seconds);

    results_output_file = fopen("resultados.txt", "w");
    if (results_output_file)
    {
        fprintf(results_output_file, "FEMENINO = %f\n", female_average_purchase);
        fprintf(results_output_file, "MASCULINO = %f\n", male_average_purchase);
        fprintf(results_output_file, "TIEMPO = %f segundos\n", execution_time_seconds);
        fclose(results_output_file);
    }
    else
    {
        #pragma omp critical(log_write)
        {
            fprintf(log_file, "Error: cannot write resultados.txt\n");
            fflush(log_file);
        }
    }
}
