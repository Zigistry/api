#include "../include/libsql.h"
// doing #include <crow.h> works,
// but added this to maintain cross-platformity
#include "../include/crow_all.h"

std::string search_packages() { return "Hello, world"; }

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
