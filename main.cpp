#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

// Callback 1: Appends incoming data (like the directory listing) to a
// std::string
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb,
                                  void *userp) {
  size_t realsize = size * nmemb;
  std::string *mem = static_cast<std::string *>(userp);
  mem->append(static_cast<char *>(contents), realsize);
  return realsize;
}

// Callback 2: Writes incoming data (the CSV contents) directly to a local file
// stream
static size_t WriteFileCallback(void *ptr, size_t size, size_t nmemb,
                                void *stream) {
  std::ofstream *out = static_cast<std::ofstream *>(stream);
  out->write(static_cast<char *>(ptr), size * nmemb);
  return size * nmemb;
}

void descargar_archivos() {
  CURL *curl;
  CURLcode res;
  std::string dir_listing;

  curl = curl_easy_init();
  if (curl) {
    // Step 1: Configure SFTP connection for the root directory
    std::string base_url = "sftp://137.184.45.251/";
    curl_easy_setopt(curl, CURLOPT_URL, base_url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD, "utem:CPyD.2026");

    // Disable strict host and peer verification for this environment
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Ask for the directory listing only
    curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &dir_listing);

    std::cout << "Connecting to SFTP server to fetch directory listing...\n";
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      std::cerr << "Error listing directory: " << curl_easy_strerror(res)
                << "\n";
    } else {
      // Step 2: Parse the string to find valid CSV files
      // Matches any file starting with "reporte_" ending in ".csv"
      std::regex csv_regex("(reporte_\\d+\\.csv)");
      std::smatch match;
      std::string::const_iterator searchStart(dir_listing.cbegin());
      std::vector<std::string> files_to_download;

      while (std::regex_search(searchStart, dir_listing.cend(), match,
                               csv_regex)) {
        files_to_download.push_back(match[1]);
        searchStart = match.suffix().first; // Move search forward
      }

      std::cout << "Found " << files_to_download.size()
                << " CSV files to download.\n";

      // Step 3: Loop through and download each file
      for (const auto &filename : files_to_download) {
        std::string file_url = base_url + filename;
        std::ofstream outfile(filename, std::ios::binary);

        if (!outfile) {
          std::cerr << "Could not create local file: " << filename << "\n";
          continue;
        }

        std::cout << "Downloading " << filename << "...\n";

        // Reconfigure cURL for downloading the specific file
        curl_easy_setopt(curl, CURLOPT_URL, file_url.c_str());
        curl_easy_setopt(curl, CURLOPT_DIRLISTONLY,
                         0L); // Turn off directory listing
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteFileCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outfile);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
          std::cerr << "Error al descargar " << filename << ": "
                    << curl_easy_strerror(res)
                    << "\n"; // Error logging template
        }

        outfile.close();
      }
    }
    curl_easy_cleanup(curl);
  }
}

int main() {
  // Required to be called once before any cURL functions
  curl_global_init(CURL_GLOBAL_DEFAULT);

  descargar_archivos();

  curl_global_cleanup();
  return 0;
}