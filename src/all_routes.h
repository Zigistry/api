#include <iostream>
#include "../include/crow_all.h"
#include <algorithm>
#include <cmath>



crow::response search(const crow::request& req, const std::string query_str);
crow::response infinite_scroll(const crow::request& req, const std::string query_str);
crow::response get_user_route(const crow::request& req);
crow::response packageIndexDetails(const crow::request& req);

