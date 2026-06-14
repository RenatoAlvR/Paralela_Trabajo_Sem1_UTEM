#ifndef UTILS_HPP
#define UTILS_HPP

// Declaración del puntero global de logs y la función de escritura en memoria para CURL.

#include <cstdio>
#include <string>

using namespace std;

extern FILE *log_file;

// Función de callback para curl que escribe los datos recibidos de la red en un buffer de memoria de tipo string.
size_t write_memory_callback(void *received_content, size_t element_size, size_t element_count, void *user_pointer);

#endif // UTILS_HPP
