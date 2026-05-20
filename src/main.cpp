#include "../include/libsql.h"
// doing #include <crow.h> works,
// but added this to maintain cross-platformity
#include "../include/crow_all.h"
#include "./all_routes.h"
#include "./helper_lib/helper_lib.h"
#include <iostream>
#include <string>

libsql_connection_t database_connection;

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

    crow::App<crow::CORSHandler> app;

    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods("GET"_method, "POST"_method, "OPTIONS"_method)
        .headers("Content-Type", "Authorization");

    CROW_ROUTE(app, "/search/packages/")
    ([](const crow::request& req) {
        return search(req, search_packages_database_query);
    });
    CROW_ROUTE(app, "/search/programs/")
    ([](const crow::request& req) {
        return search(req, search_programs_database_query);
    });
    CROW_ROUTE(app, "/packages/scroll/")
    ([](const crow::request& req) {
        return infinite_scroll(req, infinite_scroll_packages_query);
    });
    CROW_ROUTE(app, "/programs/scroll/")
    ([](const crow::request& req) {
        return infinite_scroll(req, infinite_scroll_programs_query);
    });
    CROW_ROUTE(app, "/users/")
    (get_user_route);
    CROW_ROUTE(app, "/packageIndexDetails/")
    (packageIndexDetails);
    app.port(7860).multithreaded().run();
}
