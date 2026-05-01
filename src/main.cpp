#include "../include/libsql.h"
// doing #include <crow.h> works,
// but added this to maintain cross-platformity
#include "../include/crow_all.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <string>

libsql_connection_t database_connection;

crow::response search_packages(const crow::request& req)
{
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
        search_query = raw_per_page;
        page = std::stoul(raw_page);
        per_page = std::stoul(raw_per_page);
    } catch (...) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Parameters are not in the correct format";
        return crow::response(error_responce);
    }

    if (per_page > 10) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "per_page can't be greater than 10";
        return crow::response(error_responce);
    }

    if (search_query.length() > 200) {
        crow::json::wvalue error_responce;
        error_responce["error"] = "Search query length can't be greater than 200";
        return crow::response(error_responce);
    }

    if (page - 10 != 0 and per_page > INT_MAX / page - 10) {
        // I thought, if we do a * b,
        // and it turns out to be larger
        // than what int can store?
        // this is just a check for checking
        // if long * long < INT_MAX
        crow::json::wvalue error_responce;
        error_responce["error"] = "Very long integers";
        return crow::response(error_responce);
    }

    const unsigned int start_index = page * per_page;
    const unsigned int end_index = start_index + per_page;

    // this will convert something like:
    // hello
    // into:
    // '%hello%'

    const std::string query = std::format("'%{}%'", search_query);

    std::string search_packages_count_query = std::format(R"""(


            SELECT COUNT(*) FROM repos
            WHERE is_disabled = 0
            
            AND EXISTS (
                SELECT 1 FROM packages WHERE packages.repo_id = repos.id
            )

            AND (
                EXISTS (
                    SELECT 1 FROM repo_search
                    WHERE repo_search.repo_id = repos.id
                    AND keywords MATCH {}
                )

                OR repos.id LIKE {}
                OR repos.owner LIKE {}
                OR repos.description LIKE {}
                OR repos.primary_language LIKE {}
            );


    )""",
        query, query, query, query, query);

    const libsql_statement_t count_stmt = libsql_connection_prepare(
        database_connection, search_packages_count_query.c_str());

    unsigned int total_results = 0;

    if (not count_stmt.err) {
        const libsql_rows_t count_rows = libsql_statement_query(count_stmt);
        const libsql_row_t count_row = libsql_rows_next(count_rows);
        if (not count_row.err and not libsql_row_empty(count_row)) {
            total_results = (unsigned int)libsql_row_value(count_row, 0).ok.value.integer;
        }
    }

    if (total_results == 0) {
        crow::json::wvalue empty_responce;
        return crow::response(empty_responce);
    }

    // here, we can get like, 75 / 10
    // i.e 7.5, hence this would require 8 pages.
    // i.e ceil(7.5) -> 8
    const float pages_needed_in_decimal = (float)total_results / per_page;

    const unsigned long total_pages = std::ceil(pages_needed_in_decimal);

    // either the first page, or the requested one
    // also, to make sure page is in range
    const unsigned long page_needed = std::max(1ul, std::min(total_pages, page));

    const unsigned long offset = (page_needed - 1) * per_page;

    const std::string search_packages_database_query = std::format(R"""(


        SELECT
            repos.id, users.avatar_id, repos.owner, repos.platform, repos.description,
            repos.issues_count, repos.default_branch_name, repos.fork_count,
            repos.stargazer_count, repos.watchers_count, repos.pushed_at, repos.created_at,
            repos.is_archived, repos.is_disabled, repos.is_fork, repos.license,
            repos.primary_language,
            (
                SELECT minimum_zig_version FROM releases
                WHERE repo_id = repos.id AND version = "__ZIGISTRY__DEFAULT__BRANCH__"
            ) AS minimum_zig_version,

            (
                SELECT COUNT(*) FROM repo_dependents
                WHERE repo_id = repos.id
            ) AS dependents_count
        FROM repos
    	  LEFT JOIN users ON repos.owner = users.id
        WHERE repos.is_disabled = 0
  
        AND EXISTS (
            SELECT 1 FROM packages
            WHERE packages.repo_id = repos.id
        )
  
        AND (
            EXISTS (
                SELECT 1 FROM repo_search
                WHERE repo_search.repo_id = repos.id
                AND repo_search.keywords MATCH {}
            )
            OR repos.id LIKE {}
            OR repos.owner LIKE {}
            OR repos.description LIKE {}
            OR repos.primary_language LIKE {}
        )
        ORDER BY repos.stargazer_count DESC
        LIMIT {} OFFSET {};


    )""",
        query, query, query, query, query, per_page, offset);

    crow::json::wvalue normal_responce;
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
