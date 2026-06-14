#include "csv_parser.hpp"
#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <exception>

using namespace std;

// Procesa un archivo CSV, validando su estructura y extrayendo la lista de transacciones.
vector<Transaction> parse_csv_file(const string &csv_file_path)
{
    vector<Transaction> parsed_transactions;
    ifstream input_file_stream;
    string current_line;
    int current_line_number;
    vector<string> row_columns;
    stringstream line_stream;
    string current_cell;
    string amount_cell;
    string uuid_cell;
    float amount_float;

    input_file_stream.open(csv_file_path);
    if (!input_file_stream.is_open())
    {
        #pragma omp critical(log_write)
        {
            fprintf(log_file, "Error: Cannot open %s\n", csv_file_path.c_str());
            fflush(log_file);
        }
        return parsed_transactions;
    }

    if (getline(input_file_stream, current_line))
    {
        if (!current_line.empty() && current_line.back() == '\r')
        {
            current_line.pop_back();
        }
        if (current_line.find("MONTO APLICADO") == string::npos || current_line.find("CODIGO CLIENTE") == string::npos)
        {
            #pragma omp critical(log_write)
            {
                fprintf(log_file, "Error: Bad header in %s\n", csv_file_path.c_str());
                fflush(log_file);
            }
            return parsed_transactions;
        }
    }
    else
    {
        #pragma omp critical(log_write)
        {
            fprintf(log_file, "Error: Empty file %s\n", csv_file_path.c_str());
            fflush(log_file);
        }
        return parsed_transactions;
    }

    current_line_number = 1;
    while (getline(input_file_stream, current_line))
    {
        current_line_number++;
        if (!current_line.empty() && current_line.back() == '\r')
        {
            current_line.pop_back();
        }

        row_columns.clear();
        line_stream.clear();
        line_stream.str(current_line);

        while (getline(line_stream, current_cell, ';'))
        {
            if (!current_cell.empty() && current_cell.front() == '"')
            {
                current_cell.erase(0, 1);
            }
            if (!current_cell.empty() && current_cell.back() == '"')
            {
                current_cell.pop_back();
            }
            row_columns.push_back(current_cell);
        }

        if (!current_line.empty() && current_line.back() == ';')
        {
            row_columns.push_back("");
        }

        if (row_columns.size() != 10)
        {
            #pragma omp critical(log_write)
            {
                fprintf(log_file, "Error: %s line %d cols %zu\n", csv_file_path.c_str(), current_line_number, row_columns.size());
                fflush(log_file);
            }
            return vector<Transaction>();
        }

        amount_cell = row_columns[6];
        uuid_cell = row_columns[9];

        try
        {
            amount_float = stof(amount_cell);
            parsed_transactions.push_back({uuid_cell, amount_float});
        }
        catch (const exception &conversion_exception)
        {
            #pragma omp critical(log_write)
            {
                fprintf(log_file, "Error: Bad MONTO %s line %d: %s\n", csv_file_path.c_str(), current_line_number, conversion_exception.what());
                fflush(log_file);
            }
            return vector<Transaction>();
        }
    }

    return parsed_transactions;
}
