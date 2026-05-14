#include "./helper_lib/helper_lib.h"

extern libsql_connection_t database_connection;

crow::response scroll(const crow::request& req, const std::string query_str)
{
    auto start = std::chrono::steady_clock::now();
    const char* raw_page_search = req.url_params.get("per_page");
    const char* raw_page = req.url_params.get("page");


    

}
