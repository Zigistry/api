#include "./helper_lib/helper_lib.h"

extern sqlite3* database_connection;

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
    sqlite3_stmt* query_stmt = nullptr;
    int rc = sqlite3_prepare_v2(database_connection, query_str.c_str(), -1, &query_stmt, nullptr);

    if (rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(rc) << std::endl;
        sqlite3_finalize(query_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    int bind_rc;
    bind_rc = sqlite3_bind_int64(query_stmt, 1, (sqlite3_int64)per_page);
    if (bind_rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(bind_rc) << std::endl;
        sqlite3_finalize(query_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    bind_rc = sqlite3_bind_int64(query_stmt, 2, (sqlite3_int64)offset);
    if (bind_rc != SQLITE_OK) {
        std::cout << sqlite3_errstr(bind_rc) << std::endl;
        sqlite3_finalize(query_stmt);
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server. 1";
        return crow::response(error_responce);
    }

    crow::json::wvalue normal_responce;

    crow::json::wvalue::list items;

    while (true) {
        int r = sqlite3_step(query_stmt);
        if (r != SQLITE_ROW && r != SQLITE_DONE) {
            sqlite3_finalize(query_stmt);
            crow::json::wvalue error_responce;
            error_responce["error"] = "Problem with server.";
            return crow::response(error_responce);
        }

        if (r == SQLITE_DONE) {
            break;
        }

        crow::json::wvalue item;
        std::string id = get_row_text(query_stmt, 0);

        item["id"] = id;

        item["avatar_url"] = get_row_text(query_stmt, 1);
        item["owner_name"] = get_row_text(query_stmt, 2);

        item["repo_name"] = adv_tokenizer(id, '/', 2);
        item["owner"] = get_row_text(query_stmt, 2);

        std::string provider = get_row_text(query_stmt, 3);
        item["description"] = get_row_text(query_stmt, 4);
        item["platform"] = get_row_text(query_stmt, 3);
        item["provider"] = provider == "github" ? "gh" : "cb";
        item["issues_count"] = GET_ROW_UL(query_stmt, 5);
        item["default_branch_name"] = get_row_text(query_stmt, 6);
        item["fork_count"] = GET_ROW_UL(query_stmt, 7);
        item["stargazer_count"] = GET_ROW_UL(query_stmt, 8);
        item["watchers_count"] = GET_ROW_UL(query_stmt, 9);
        item["pushed_at"] = get_row_text(query_stmt, 10);
        item["created_at"] = get_row_text(query_stmt, 11);
        item["is_archived"] = (bool)GET_ROW_UL(query_stmt, 12);
        item["is_disabled"] = (bool)GET_ROW_UL(query_stmt, 13);
        item["is_fork"] = (bool)GET_ROW_UL(query_stmt, 14);
        item["license"] = get_row_text(query_stmt, 15);
        item["primary_language"] = get_row_text(query_stmt, 16);
        item["minimum_zig_version"] = get_row_text(query_stmt, 17) == "" ? "0.0.0" : get_row_text(query_stmt, 17);
        item["dependents_count"] = GET_ROW_UL(query_stmt, 18);

        items.push_back(item);
    }

    sqlite3_finalize(query_stmt);

    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    normal_responce = std::move(items);

    return crow::response(normal_responce);
}
