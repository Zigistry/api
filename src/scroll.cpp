#include "./helper_lib/helper_lib.h"

extern libsql_connection_t database_connection;

crow::response scroll(const crow::request& req, const std::string query_str)
{
    auto start = std::chrono::steady_clock::now();
    const char* raw_per_page = req.url_params.get("per_page");
    const char* raw_page = req.url_params.get("page");

    if (raw_page == NULL or raw_per_page == NULL) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Parameters needed: q, page and per_page.";
        return crow::response(error_responce);
    }

    unsigned long page;
    unsigned long per_page;
    try {
        page = std::max(std::stoul(raw_page), 1ul);
        per_page = std::max(std::stoul(raw_per_page), 1ul);

        per_page = std::min(per_page, 12UL);
        page = std::min(page, 1000000UL);
    } catch (...) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Parameters are not in the correct format";
        return crow::response(error_responce);
    }

    const unsigned long offset = (page - 1) * per_page;
    const libsql_statement_t query_stmt = libsql_connection_prepare(database_connection, query_str.c_str());


}
