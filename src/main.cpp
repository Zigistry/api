#include "../include/libsql.h"
// doing #include <crow.h> works,
// but added this to maintain cross-platformity
#include "../include/crow_all.h"
#include "./all_routes.h"
#include "./helper_lib/helper_lib.h"
#include <iostream>
#include <string>
#include <mutex>

libsql_connection_t database_connection;
std::mutex db_mutex;

int main()
{
    libsql_setup(libsql_config_t {});

    const libsql_database_t db = libsql_database_init((libsql_database_desc_t) {
        .path = "zigistry.db",
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

    CROW_ROUTE(app, "/programIndexDetails/")
    (programIndexDetails);

    CROW_ROUTE(app, "/packageIndexDetails/")
    (packageIndexDetails);

    CROW_ROUTE(app, "/packages/")
    ([](const crow::request& req) {
        return package_details(req);
    });

    CROW_ROUTE(app, "/programs/")
    ([](const crow::request& req) {
        return package_details(req);
    });
    app.port(7860).multithreaded().run();
}
