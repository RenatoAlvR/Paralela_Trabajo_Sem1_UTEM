#ifndef SFTP_MANAGER_HPP
#define SFTP_MANAGER_HPP

// Orquestación de descargas concurrentes de reportes mediante tareas de OpenMP desde un servidor SFTP.

#include <vector>
#include "csv_parser.hpp"

using namespace std;

// Conecta al servidor SFTP, obtiene la lista de reportes CSV y los descarga concurrentemente usando tareas de OpenMP.
void download_sftp_files(vector<Transaction> &all_transactions_list);

#endif // SFTP_MANAGER_HPP
