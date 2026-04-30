#include "../include/libsql.h"
// doing #include <crow.h> works,
// but added this to maintain cross-platformity
#include "../include/crow_all.h"
#include <climits>
#include <string>

crow::response search_packages(const crow::request &req) {
  const char *raw_search_query = req.url_params.get("q");
  const char *raw_page = req.url_params.get("page");
  const char *raw_per_page = req.url_params.get("per_page");

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


  if (page != 0 and per_page > INT_MAX / page) {
    // I thought, if we do a * b,
    // and it turns out to be larger
    // than what int can store?
    // this is just a check for checking
    // if long * long < INT_MAX
    crow::json::wvalue error_responce;
    error_responce["error"] = "Very long integers";
    return crow::response(error_responce);
  }

  crow::json::wvalue normal_responce;
  return crow::response(normal_responce);
}

int main() {
  const char *database_url = getenv("DATABASE_URL");
  const char *api_key = getenv("API_KEY");

  if (database_url == NULL || api_key == NULL) {
    printf("Oh no! Problem! No env keys\n");
    return 1;
  }

  libsql_setup(libsql_config_t{});

  const libsql_database_t db = libsql_database_init((libsql_database_desc_t){
      .url = database_url,
      .path = "local.db",
      .auth_token = api_key,
      // every hour
      .sync_interval = 1000 * 60 * 60,
      .synced = true,
  });

  if (db.err) {
    fprintf(stderr, "Error: %s\n", libsql_error_message(db.err));
    return 1;
  }
  libsql_connection_t database_connection = libsql_database_connect(db);
  printf("CONNECTED!!!!\n");
  if (database_connection.err) {
    fprintf(stderr, "Connection error: %s\n",
            libsql_error_message(database_connection.err));
    return 1;
  }

  crow::SimpleApp app;

  CROW_ROUTE(app, "/search/packages/")(search_packages);

  app.port(3000).multithreaded().run();
}
