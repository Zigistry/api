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

    if (search_query.size() > 200) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Search query length can't be greater than 200";
        return crow::response(error_responce);
    }

    std::string like_query = "%" + search_query + "%";

    const unsigned long offset = (page - 1) * per_page;

    const std::string search_packages_database_query = R"""(


            WITH filtered AS MATERIALIZED (
                SELECT r.id
                FROM repos r
                WHERE r.is_disabled = 0
                  AND EXISTS (SELECT 1 FROM packages p WHERE p.repo_id = r.id)
                  AND (
                      r.id IN (SELECT repo_id FROM repo_search WHERE keywords MATCH ?)
                      OR r.id LIKE ?
                      OR r.owner LIKE ?
                      OR r.description LIKE ?
                      OR r.primary_language LIKE ?
                  )
            ),
            paged AS MATERIALIZED (
                SELECT id FROM filtered LIMIT ? OFFSET ?
            )
            SELECT
                r.id, u.avatar_id, r.owner, r.platform, r.description,
                r.issues_count, r.default_branch_name, r.fork_count,
                r.stargazer_count, r.watchers_count, r.pushed_at, r.created_at,
                r.is_archived, r.is_disabled, r.is_fork, r.license,
                r.primary_language,
                rel.minimum_zig_version,
                COALESCE(dep.dependents_count, 0) AS dependents_count,
                (SELECT COUNT(*) FROM filtered) AS total_results
            FROM paged
            JOIN repos r ON r.id = paged.id
            LEFT JOIN users u ON u.id = r.owner
            LEFT JOIN releases rel ON rel.repo_id = r.id AND rel.version = '__ZIGISTRY__DEFAULT__BRANCH__'
            LEFT JOIN (
                SELECT repo_id, COUNT(*) AS dependents_count
                FROM repo_dependents
                WHERE repo_id IN (SELECT id FROM paged)
                GROUP BY repo_id
            ) dep ON dep.repo_id = r.id;


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

    unsigned int total_results = 0;
    bool if_read = false;

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
        if (!if_read) {
            total_results = (unsigned int)libsql_row_value(row, 19).ok.value.integer;
            if_read = true;
        }
    }

    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    normal_responce["total"] = total_results;
    normal_responce["page"] = page;
    normal_responce["per_page"] = per_page;
    normal_responce["total_pages"] = std::ceil((float)total_results / per_page);
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
