#include "./helper_lib/helper_lib.h"

extern libsql_connection_t database_connection;

// https://www.geeksforgeeks.org/java/how-to-split-a-string-in-cc-python-and-java/

crow::response infinite_scroll(const crow::request& req, const std::string query_str)
{
    std::lock_guard<std::mutex> lock(db_mutex);
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

    if (query_stmt.err) {
        std::cout << libsql_error_message(query_stmt.err) << std::endl;
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    libsql_statement_bind_value(
        query_stmt,
        libsql_integer(
            per_page));

    libsql_statement_bind_value(
        query_stmt,
        libsql_integer(
            offset));

    if (query_stmt.err) {
        std::cout << libsql_error_message(query_stmt.err) << std::endl;
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    const libsql_rows_t rows = libsql_statement_query(query_stmt);

    if (rows.err) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 5";
        return crow::response(error_responce);
    }

    crow::json::wvalue normal_responce;

    crow::json::wvalue::list items;

    while (true) {
        libsql_row_t row;
        if ((row = libsql_rows_next(rows)).err) {
            crow::json::wvalue error_responce;
            error_responce["error"] = "Problem with server.";
            return crow::response(error_responce);
        }

        if (libsql_row_empty(row)) {
            break;
        }

        crow::json::wvalue item;
        std::string id = get_row_text(row, 0);

        item["id"] = id;

        item["avatar_url"] = get_row_text(row, 1);
        item["owner_name"] = get_row_text(row, 2);

        item["repo_name"] = adv_tokenizer(id, '/', 2);
        item["owner"] = get_row_text(row, 2);

        std::string provider = get_row_text(row, 3);
        item["description"] = get_row_text(row, 4);
        item["platform"] = get_row_text(row, 3);
        item["provider"] = provider == "github" ? "gh" : "cb";
        item["issues_count"] = GET_ROW_UL(row, 5);
        item["default_branch_name"] = get_row_text(row, 6);
        item["fork_count"] = GET_ROW_UL(row, 7);
        item["stargazer_count"] = GET_ROW_UL(row, 8);
        item["watchers_count"] = GET_ROW_UL(row, 9);
        item["pushed_at"] = get_row_text(row, 10);
        item["created_at"] = get_row_text(row, 11);
        item["is_archived"] = (bool)GET_ROW_UL(row, 12);
        item["is_disabled"] = (bool)GET_ROW_UL(row, 13);
        item["is_fork"] = (bool)GET_ROW_UL(row, 14);
        item["license"] = get_row_text(row, 15);
        item["primary_language"] = get_row_text(row, 16);
        item["minimum_zig_version"] = get_row_text(row, 17) == "" ? "0.0.0" : get_row_text(row, 17);
        item["dependents_count"] = GET_ROW_UL(row, 18);

        items.push_back(item);
    }

    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    normal_responce = std::move(items);

    return crow::response(normal_responce);
}
