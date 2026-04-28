#include "../include/libsql.h"
// doing #include <crow.h> works,
// but added this to maintain cross-platformity
#include "../include/crow_all.h"

int main() {
  crow::SimpleApp app;

  CROW_ROUTE(app, "/")([]() { return "Hello, World!"; });

  app.port(3000).multithreaded().run();
}
