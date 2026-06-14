#ifndef CSV_PARSER_HPP
#define CSV_PARSER_HPP

// Estructura de datos de transacciones y función para el procesamiento y validación de archivos CSV.

#include <string>
#include <vector>

using namespace std;

struct Transaction
{
    string client_uuid;
    float purchase_amount_clp;
};

// Procesa un archivo CSV, validando su estructura y extrayendo la lista de transacciones.
vector<Transaction> parse_csv_file(const string &csv_file_path);

#endif // CSV_PARSER_HPP
