#include "../include/libsql.h"
// doing #include <crow.h> works,
// but added this to maintain cross-platformity
#include "../include/crow_all.h"
#include <algorithm>
#include <cmath>
#include <string>

#define GET_ROW(A, B)

libsql_connection_t database_connection;

crow::response search_packages(const crow::request& req)
{
    auto start = std::chrono::steady_clock::now();
    const char* raw_search_query = req.url_params.get("q");
    const char* raw_page = req.url_params.get("page");
    const char* raw_per_page = req.url_params.get("per_page");

    if (raw_search_query == NULL or raw_page == NULL or raw_per_page == NULL) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Parameters needed: q, page and per_page.";
        return crow::response(error_responce);
    }
    std::string search_query;
    unsigned long page;
    unsigned long per_page;
    try {
        search_query = raw_search_query;
        page = std::max(std::stoul(raw_page), 1ul);
        per_page = std::max(std::stoul(raw_per_page), 1ul);

        per_page = std::min(per_page, 10UL);
        page = std::min(page, 1000000UL);
    } catch (...) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Parameters are not in the correct format";
        return crow::response(error_responce);
    }

    if (search_query.length() > 200) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Search query length can't be greater than 200";
        return crow::response(error_responce);
    }

    

    std::string like_query = "%" + search_query + "%";

    std::string search_packages_count_query = R"""(

            SELECT COUNT(*)
            FROM repos r
            WHERE r.is_disabled = 0
            AND (
                EXISTS (
                    SELECT 1 FROM repo_search
                    WHERE keywords MATCH ?
                )
                OR r.id LIKE ?
                OR r.owner LIKE ?
                OR r.description LIKE ?
                OR r.primary_language LIKE ?
            )
            AND EXISTS (
                SELECT 1 FROM packages p
                WHERE p.repo_id = r.id
            );


    )""";

    const libsql_statement_t count_stmt = libsql_connection_prepare(
        database_connection, search_packages_count_query.c_str());
    libsql_statement_bind_value(
        count_stmt,
        libsql_text(
            search_query.c_str(),
            search_query.length()));

    for (int i = 0; i < 4; i++) {
        libsql_statement_bind_value(
            count_stmt,
            libsql_text(
                like_query.c_str(),
                like_query.length()));
    }

    unsigned int total_results = 0;

    if (not count_stmt.err) {
        const libsql_rows_t count_rows = libsql_statement_query(count_stmt);
        const libsql_row_t count_row = libsql_rows_next(count_rows);
        if (not count_row.err and not libsql_row_empty(count_row)) {
            total_results = (unsigned int)libsql_row_value(count_row, 0).ok.value.integer;
        } else {
            crow::json::wvalue error_responce;
            error_responce["error"] = "Problem with server.4";
            return crow::response(error_responce);
        }
    } else {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Problem with server.3";
        return crow::response(error_responce);
    }

    // here, we can get like, 75 / 10
    // i.e 7.5, hence this would require 8 pages.
    // i.e ceil(7.5) -> 8
    const float pages_needed_in_decimal = (float)total_results / per_page;

    const unsigned long total_pages = std::ceil(pages_needed_in_decimal);

    // either the first page, or the requested one
    // also, to make sure page is in range
    // total_pages can become 0
    const unsigned long page_needed = std::max(1UL, std::min(total_pages, page));

    const unsigned long offset = (page_needed - 1) * per_page;

    const std::string search_packages_database_query = R"""(

    SELECT
        r.id, u.avatar_id, r.owner, r.platform, r.description,
        r.issues_count, r.default_branch_name, r.fork_count,
        r.stargazer_count, r.watchers_count, r.pushed_at, r.created_at,
        r.is_archived, r.is_disabled, r.is_fork, r.license,
        r.primary_language,
    (
        SELECT minimum_zig_version
        FROM releases
        WHERE repo_id = r.id
        AND version = '__ZIGISTRY__DEFAULT__BRANCH__'
        LIMIT 1
    ) AS minimum_zig_version,
    (
        SELECT COUNT(*)
        FROM repo_dependents d
        WHERE d.repo_id = r.id
    ) AS dependents_count

    FROM repos r
    LEFT JOIN users u ON r.owner = u.id
    WHERE r.is_disabled = 0
      AND EXISTS (
      SELECT 1
      FROM packages p
      WHERE p.repo_id = r.id
    )
    AND (
        r.id IN (
            SELECT repo_id
            FROM repo_search
            WHERE keywords MATCH ?
        )
        OR r.id LIKE ?
        OR r.owner LIKE ?
        OR r.description LIKE ?
        OR r.primary_language LIKE ?
    )
    GROUP BY r.id
    LIMIT ? OFFSET ?;


       
    )""";

    const libsql_statement_t query_stmt = libsql_connection_prepare(database_connection, search_packages_database_query.c_str());

    libsql_statement_bind_value(
        query_stmt,
        libsql_text(
            search_query.c_str(),
            search_query.length()));
    for (int i = 0; i < 4; i++) {
        libsql_statement_bind_value(
            query_stmt,
            libsql_text(
                like_query.c_str(),
                like_query.length()));
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
    }

    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    normal_responce["total"] = total_results;
    normal_responce["page"] = page;
    normal_responce["per_page"] = per_page;
    normal_responce["total_pages"] = total_pages;
    normal_responce["time_took_to_search_ns"] = duration.count();

    return crow::response(normal_responce);
}

int main()
{
    const char* database_url = getenv("DATABASE_URL");
    const char* api_key = getenv("API_KEY");

    if (database_url == NULL or api_key == NULL) {
        std::cout << "Oh no! Problem! No env keys" << std::endl;
        return 1;
    }

    libsql_setup(libsql_config_t {});

    const libsql_database_t db = libsql_database_init((libsql_database_desc_t) {
        .url = database_url,
        .path = "local.db",
        .auth_token = api_key,
        // every hour
        .sync_interval = 1000 * 60 * 60,
        .synced = true,
    });

    if (db.err) {
        std::cerr << "Error:\n"
                  << libsql_error_message(db.err) << std::endl;
        return 1;
    }

    database_connection = libsql_database_connect(db);

    std::cout << "Connected..." << std::endl;

    if (database_connection.err) {
        std::cerr << "Connection error: " << libsql_error_message(database_connection.err) << std::endl;
        return 1;
    }

    crow::SimpleApp app;
    CROW_ROUTE(app, "/search/packages/")(search_packages);
    app.port(3000).multithreaded().run();
}
