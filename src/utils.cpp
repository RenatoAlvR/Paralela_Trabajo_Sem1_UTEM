#include "utils.hpp"

using namespace std;

FILE *log_file = nullptr;

// Función de callback para curl que escribe los datos recibidos de la red en un buffer de memoria de tipo string.
size_t write_memory_callback(void *received_content, size_t element_size, size_t element_count, void *user_pointer)
{
    size_t total_size_bytes;
    string *destination_buffer;

    total_size_bytes = element_size * element_count;
    destination_buffer = static_cast<string *>(user_pointer);
    destination_buffer->append(static_cast<char *>(received_content), total_size_bytes);
    return total_size_bytes;
}
