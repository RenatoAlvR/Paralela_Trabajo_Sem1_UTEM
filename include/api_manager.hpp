#ifndef API_MANAGER_HPP
#define API_MANAGER_HPP

// Consulta concurrente multihilo de géneros mediante solicitudes REST HTTP y almacenamiento en caché.

#include <set>
#include <string>
#include <unordered_map>

using namespace std;

// Consulta de forma concurrente el género de los clientes asociados a un conjunto de UUIDs y los almacena en el caché.
void query_client_api(const set<string> &unique_uuids_set, unordered_map<string, string> &client_genders_cache);

#endif // API_MANAGER_HPP
