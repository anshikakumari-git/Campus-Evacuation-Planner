
#include "httplib.h"
#include <iostream>
using namespace std;

int main() {

    httplib::Server server;

    server.Get("/", [](const httplib::Request& req,
                       httplib::Response& res) {

        res.set_content(
            "Campus Evacuation Planner Server is Working!",
            "text/plain"
        );

    });

    cout << "Server running at http://localhost:8080\n";

    server.listen("127.0.0.1", 8080);

    return 0;
}
