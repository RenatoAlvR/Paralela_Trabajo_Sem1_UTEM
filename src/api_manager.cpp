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

// ============================================================
// Credenciales y token JWT global
// ============================================================
static string current_jwt_token = "";
const string API_EMAIL = "ralvarezra@utem.cl";
const string API_RUT   = "21.484.615-2";

// ============================================================
// Función de autenticación contra la API
// POST https://api.sebastian.cl/cpyd/v1/login/authenticate
// Payload: {"email": "...", "rut": "..."}
// Retorna el JWT extraído de la respuesta JSON.
// ============================================================
string authenticate_api_user()
{
    CURL *auth_curl_handle;
    CURLcode auth_curl_result;
    string auth_response;
    long auth_http_status;
    struct curl_slist *auth_headers = nullptr;
    string jwt_token = "";

    auth_curl_handle = curl_easy_init();
    if (!auth_curl_handle)
    {
        fprintf(stderr, "ERROR: No se pudo inicializar CURL para autenticación.\n");
        return "";
    }

    // Construir el payload JSON
    string json_payload = "{\"email\": \"" + API_EMAIL + "\", \"rut\": \"" + API_RUT + "\"}";

    // Configurar headers
    auth_headers = curl_slist_append(auth_headers, "Content-Type: application/json");

    // Configurar la solicitud POST
    curl_easy_setopt(auth_curl_handle, CURLOPT_URL, "https://api.sebastian.cl/cpyd/v1/login/authenticate");
    curl_easy_setopt(auth_curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(auth_curl_handle, CURLOPT_POSTFIELDS, json_payload.c_str());
    curl_easy_setopt(auth_curl_handle, CURLOPT_HTTPHEADER, auth_headers);
    curl_easy_setopt(auth_curl_handle, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(auth_curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(auth_curl_handle, CURLOPT_WRITEDATA, &auth_response);
    curl_easy_setopt(auth_curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(auth_curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);

    auth_curl_result = curl_easy_perform(auth_curl_handle);
    auth_http_status = 0;
    curl_easy_getinfo(auth_curl_handle, CURLINFO_RESPONSE_CODE, &auth_http_status);

    if (auth_curl_result != CURLE_OK)
    {
        fprintf(stderr, "ERROR: Autenticación falló (curl): %s\n", curl_easy_strerror(auth_curl_result));
    }
    else if (auth_http_status != 200)
    {
        fprintf(stderr, "ERROR: Autenticación falló (HTTP %ld). Respuesta: %s\n", auth_http_status, auth_response.c_str());
    }
    else
    {
        // Extraer el JWT de la respuesta JSON.
        // La respuesta puede ser: {"jwt": "eyJ..."} o similar.
        // Intentamos buscar la clave "jwt".
        string jwt_key = "\"jwt\"";
        size_t key_pos = auth_response.find(jwt_key);
        if (key_pos != string::npos)
        {
            // Buscar el inicio del valor (después de ':' y '"')
            size_t colon_pos = auth_response.find(':', key_pos + jwt_key.length());
            if (colon_pos != string::npos)
            {
                size_t quote_start = auth_response.find('"', colon_pos + 1);
                if (quote_start != string::npos)
                {
                    size_t quote_end = auth_response.find('"', quote_start + 1);
                    if (quote_end != string::npos)
                    {
                        jwt_token = auth_response.substr(quote_start + 1, quote_end - quote_start - 1);
                    }
                }
            }
        }

        // Si no encontramos la clave "jwt", la respuesta puede ser directamente el token como string
        if (jwt_token.empty())
        {
            // Intentar extraer como string plano (entre comillas si viene como JSON string)
            if (auth_response.size() > 2 && auth_response.front() == '"' && auth_response.back() == '"')
            {
                jwt_token = auth_response.substr(1, auth_response.size() - 2);
            }
            else if (!auth_response.empty() && auth_response.front() != '{')
            {
                // Respuesta es texto plano (el token directamente)
                jwt_token = auth_response;
            }
        }

        if (jwt_token.empty())
        {
            fprintf(stderr, "ERROR: No se pudo extraer el JWT de la respuesta: %s\n", auth_response.c_str());
        }
        else
        {
            printf("Autenticación exitosa. JWT obtenido (%zu caracteres).\n", jwt_token.size());
        }
    }

    curl_slist_free_all(auth_headers);
    curl_easy_cleanup(auth_curl_handle);

    return jwt_token;
}

// ============================================================
// Consulta concurrente de géneros con autenticación JWT
// y renovación automática del token ante 401.
// ============================================================
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

    // Paso 3: Obtener el token JWT antes de iniciar la ejecución paralela
    current_jwt_token = authenticate_api_user();
    if (current_jwt_token.empty())
    {
        fprintf(stderr, "ERROR FATAL: No se pudo obtener el token JWT inicial. Abortando consultas API.\n");
        // Llenar todo como UNKNOWN y retornar
        for (uuid_index = 0; uuid_index < uuids_vector.size(); ++uuid_index)
        {
            client_genders_cache[uuids_vector[uuid_index]] = "UNKNOWN";
        }
        return;
    }

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
        bool retry_request;
        int max_retries;

        api_query_url = "https://api.sebastian.cl/cpyd/v1/person/" + uuids_vector[uuid_index];
        retry_request = true;
        max_retries = 3;

        while (retry_request && max_retries > 0)
        {
            retry_request = false;
            max_retries--;

            // Paso 4a: Leer el token de forma segura (thread-safe)
            string local_token;
            #pragma omp critical(token_lock)
            {
                local_token = current_jwt_token;
            }

            // Paso 4b: Construir el header de Authorization
            struct curl_slist *request_headers = nullptr;
            string auth_header = "Authorization: Bearer " + local_token;
            request_headers = curl_slist_append(request_headers, auth_header.c_str());

            api_curl_handle = curl_easy_init();
            if (api_curl_handle)
            {
                api_json_response.clear();

                curl_easy_setopt(api_curl_handle, CURLOPT_URL, api_query_url.c_str());
                curl_easy_setopt(api_curl_handle, CURLOPT_TIMEOUT, 10L);
                curl_easy_setopt(api_curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
                curl_easy_setopt(api_curl_handle, CURLOPT_WRITEDATA, &api_json_response);
                curl_easy_setopt(api_curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
                curl_easy_setopt(api_curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
                curl_easy_setopt(api_curl_handle, CURLOPT_HTTPHEADER, request_headers);

                // Paso 4c: Ejecutar la solicitud
                api_curl_result = curl_easy_perform(api_curl_handle);
                http_status_code = 0;
                curl_easy_getinfo(api_curl_handle, CURLINFO_RESPONSE_CODE, &http_status_code);

                if (api_curl_result == CURLE_OK && http_status_code == 200)
                {
                    // Éxito: extraer el género
                    json_key_to_find = "\"gender\": \"";
                    key_position = api_json_response.find(json_key_to_find);
                    if (key_position == string::npos)
                    {
                        // Intentar sin espacio después de los dos puntos
                        json_key_to_find = "\"gender\":\"";
                        key_position = api_json_response.find(json_key_to_find);
                    }
                    if (key_position != string::npos)
                    {
                        key_position += json_key_to_find.length();
                        value_end_position = api_json_response.find("\"", key_position);
                        if (value_end_position != string::npos)
                        {
                            fetched_genders_vector[uuid_index] = api_json_response.substr(key_position, value_end_position - key_position);
                        }
                    }
                    // No reintentar, éxito
                }
                else if (http_status_code == 401)
                {
                    // Paso 4d: Token expirado — refrescar con doble comprobación
                    curl_easy_cleanup(api_curl_handle);
                    curl_slist_free_all(request_headers);

                    #pragma omp critical(token_lock)
                    {
                        // Solo refrescar si otro hilo no lo hizo ya
                        if (local_token == current_jwt_token)
                        {
                            printf("Token expirado. Refrescando JWT...\n");
                            current_jwt_token = authenticate_api_user();
                        }
                    }

                    retry_request = true;
                    continue; // Saltar el cleanup de abajo, ya se hizo
                }
                // Otros errores: no reintentar, queda como UNKNOWN

                // Paso 5: Liberar recursos de esta iteración
                curl_easy_cleanup(api_curl_handle);
                curl_slist_free_all(request_headers);
            }
            else
            {
                // curl_easy_init falló, liberar headers si existen
                curl_slist_free_all(request_headers);
            }
        }

        // Progreso
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
    }

    for (uuid_index = 0; uuid_index < uuids_vector.size(); ++uuid_index)
    {
        client_genders_cache[uuids_vector[uuid_index]] = fetched_genders_vector[uuid_index];
    }
}
