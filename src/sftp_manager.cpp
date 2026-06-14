#include "sftp_manager.hpp"
#include "csv_parser.hpp"
#include "utils.hpp"
#include <cstdio>
#include <curl/curl.h>
#include <omp.h>
#include <regex>
#include <string>
#include <vector>
#include <unistd.h>

using namespace std;

// Conecta al servidor SFTP, obtiene la lista de reportes CSV y los descarga/parsea concurrentemente usando hilos y reutilización de conexiones.
void download_sftp_files(vector<Transaction> &all_transactions_list)
{
    const string SFTP_SERVER_URL = "sftp://137.184.45.251/";
    const string SFTP_CREDENTIALS = "utem:CPyD.2026";
    vector<string> csv_filenames_list;

    // 1. Obtención del listado de archivos remotos usando una única conexión inicial.
    CURL *sftp_curl_handle = curl_easy_init();
    if (sftp_curl_handle)
    {
        string sftp_directory_listing;
        curl_easy_setopt(sftp_curl_handle, CURLOPT_URL, SFTP_SERVER_URL.c_str());
        curl_easy_setopt(sftp_curl_handle, CURLOPT_USERPWD, SFTP_CREDENTIALS.c_str());
        curl_easy_setopt(sftp_curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(sftp_curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(sftp_curl_handle, CURLOPT_DIRLISTONLY, 1L);
        curl_easy_setopt(sftp_curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
        curl_easy_setopt(sftp_curl_handle, CURLOPT_WRITEDATA, &sftp_directory_listing);

        printf("Fetching file list...\n");
        CURLcode sftp_curl_result = curl_easy_perform(sftp_curl_handle);

        if (sftp_curl_result != CURLE_OK)
        {
            fprintf(log_file, "Error listing: %s\n", curl_easy_strerror(sftp_curl_result));
            fflush(log_file);
            curl_easy_cleanup(sftp_curl_handle);
            return;
        }

        regex csv_filename_regex("(reporte_\\d+\\.csv)");
        smatch regex_match_result;
        string::const_iterator regex_search_iterator = sftp_directory_listing.cbegin();

        while (regex_search(regex_search_iterator, sftp_directory_listing.cend(), regex_match_result, csv_filename_regex))
        {
            csv_filenames_list.push_back(regex_match_result[1]);
            regex_search_iterator = regex_match_result.suffix().first;
        }

        printf("Found %zu CSV files on remote SFTP server\n", csv_filenames_list.size());
        curl_easy_cleanup(sftp_curl_handle);
    }
    else
    {
        return;
    }

    // 2. Descarga y parseo paralelo optimizado con reutilización de conexiones SSH (máximo 4 conexiones concurrentes).
    #pragma omp parallel num_threads(4) shared(csv_filenames_list, all_transactions_list)
    {
        CURL *download_curl_handle = curl_easy_init();
        
        #pragma omp for schedule(dynamic)
        for (size_t i = 0; i < csv_filenames_list.size(); ++i)
        {
            const string &sftp_filename = csv_filenames_list[i];
            string local_file_path = "data/" + sftp_filename;

            // Si el archivo ya existe localmente en data/, se parsea de inmediato.
            if (access(local_file_path.c_str(), F_OK) == 0)
            {
                vector<Transaction> local_transactions_list = parse_csv_file(local_file_path);
                #pragma omp critical(transaction_insert)
                {
                    all_transactions_list.insert(all_transactions_list.end(), local_transactions_list.begin(), local_transactions_list.end());
                }
            }
            // Si el archivo no existe, se descarga y parsea reutilizando la conexión activa del hilo.
            else
            {
                bool is_download_successful = false;
                const int MAX_DOWNLOAD_RETRIES = 3;

                if (download_curl_handle)
                {
                    string full_sftp_file_url = SFTP_SERVER_URL + sftp_filename;
                    
                    for (int retry = 1; retry <= MAX_DOWNLOAD_RETRIES; ++retry)
                    {
                        FILE *local_output_file = fopen(local_file_path.c_str(), "wb");
                        if (!local_output_file)
                        {
                            #pragma omp critical(log_write)
                            {
                                fprintf(log_file, "Error: create %s\n", local_file_path.c_str());
                                fflush(log_file);
                            }
                            break;
                        }

                        // Optimización de búfer y límites de tiempo para transferencias SFTP más rápidas y estables.
                        curl_easy_setopt(download_curl_handle, CURLOPT_BUFFERSIZE, 262144L); // Aumenta el búfer a 256 KB
                        curl_easy_setopt(download_curl_handle, CURLOPT_URL, full_sftp_file_url.c_str());
                        curl_easy_setopt(download_curl_handle, CURLOPT_USERPWD, SFTP_CREDENTIALS.c_str());
                        curl_easy_setopt(download_curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
                        curl_easy_setopt(download_curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
                        curl_easy_setopt(download_curl_handle, CURLOPT_DIRLISTONLY, 0L);
                        curl_easy_setopt(download_curl_handle, CURLOPT_CONNECTTIMEOUT, 15L); // Timeout de conexión de 15 segundos
                        curl_easy_setopt(download_curl_handle, CURLOPT_TIMEOUT, 300L); // Timeout de descarga de 5 minutos (evita truncamiento de archivos grandes)
                        curl_easy_setopt(download_curl_handle, CURLOPT_WRITEDATA, local_output_file);

                        CURLcode download_curl_result = curl_easy_perform(download_curl_handle);
                        fclose(local_output_file);

                        if (download_curl_result == CURLE_OK)
                        {
                            is_download_successful = true;
                            break;
                        }
                        else
                        {
                            #pragma omp critical(log_write)
                            {
                                fprintf(log_file, "Error dl %s (intento %d): %s\n", sftp_filename.c_str(), retry, curl_easy_strerror(download_curl_result));
                                fflush(log_file);
                            }
                        }
                    }
                }

                // Si la descarga fue exitosa, parsea de inmediato el archivo local.
                if (is_download_successful)
                {
                    vector<Transaction> local_transactions_list = parse_csv_file(local_file_path);
                    #pragma omp critical(transaction_insert)
                    {
                        all_transactions_list.insert(all_transactions_list.end(), local_transactions_list.begin(), local_transactions_list.end());
                    }
                }
                else
                {
                    #pragma omp critical(log_write)
                    {
                        fprintf(log_file, "Failed %s after %d tries\n", sftp_filename.c_str(), MAX_DOWNLOAD_RETRIES);
                        fflush(log_file);
                    }
                    unlink(local_file_path.c_str());
                }
            }
        }

        if (download_curl_handle)
        {
            curl_easy_cleanup(download_curl_handle);
        }
    }
}
