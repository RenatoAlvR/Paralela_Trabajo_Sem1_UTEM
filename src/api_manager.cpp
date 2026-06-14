#include "api_manager.hpp"
#include "utils.hpp"
#include <cstdio>
#include <curl/curl.h>
#include <omp.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// Consulta de forma concurrente el género de los clientes asociados a un conjunto de UUIDs y los almacena en el caché.
void query_client_api(const set<string> &unique_uuids_set, unordered_map<string, string> &client_genders_cache)
{
    vector<string> uuids_vector;
    vector<string> fetched_genders_vector;
    int completed_queries_count;
    int total_queries_to_perform;
    size_t uuid_index;

    uuids_vector.assign(unique_uuids_set.begin(), unique_uuids_set.end());
    fetched_genders_vector.resize(uuids_vector.size(), "UNKNOWN");
    completed_queries_count = 0;
    total_queries_to_perform = static_cast<int>(uuids_vector.size());

    printf("API lookups: %d UUIDs\n", total_queries_to_perform);

    #pragma omp parallel for schedule(dynamic) num_threads(48)
    for (uuid_index = 0; uuid_index < uuids_vector.size(); ++uuid_index)
    {
        string api_query_url;
        CURL *api_curl_handle;
        string api_json_response;
        CURLcode api_curl_result;
        long http_status_code;
        string json_key_to_find;
        size_t key_position;
        size_t value_end_position;
        int current_progress_count;

        api_query_url = "https://api.sebastian.cl/cpyd/v1/person/" + uuids_vector[uuid_index];
        api_curl_handle = curl_easy_init();
        if (api_curl_handle)
        {
            curl_easy_setopt(api_curl_handle, CURLOPT_URL, api_query_url.c_str());
            curl_easy_setopt(api_curl_handle, CURLOPT_TIMEOUT, 10L);
            curl_easy_setopt(api_curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
            curl_easy_setopt(api_curl_handle, CURLOPT_WRITEDATA, &api_json_response);
            curl_easy_setopt(api_curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(api_curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);

            api_curl_result = curl_easy_perform(api_curl_handle);
            http_status_code = 0;
            curl_easy_getinfo(api_curl_handle, CURLINFO_RESPONSE_CODE, &http_status_code);

            if (api_curl_result == CURLE_OK && http_status_code == 200)
            {
                json_key_to_find = "\"gender\": \"";
                key_position = api_json_response.find(json_key_to_find);
                if (key_position != string::npos)
                {
                    key_position += json_key_to_find.length();
                    value_end_position = api_json_response.find("\"", key_position);
                    if (value_end_position != string::npos)
                    {
                        fetched_genders_vector[uuid_index] = api_json_response.substr(key_position, value_end_position - key_position);
                    }
                }
            }

            #pragma omp atomic capture
            {
                completed_queries_count++;
                current_progress_count = completed_queries_count;
            }

            if (current_progress_count % 100 == 0 || current_progress_count == total_queries_to_perform)
            {
                #pragma omp critical(progress_print)
                {
                    printf("API: %d / %d\n", current_progress_count, total_queries_to_perform);
                }
            }

            curl_easy_cleanup(api_curl_handle);
        }
    }

    for (uuid_index = 0; uuid_index < uuids_vector.size(); ++uuid_index)
    {
        client_genders_cache[uuids_vector[uuid_index]] = fetched_genders_vector[uuid_index];
    }
}
