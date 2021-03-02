#include <signal.h>
#include "http_server.hpp"

#define HTTP_SERVER_PORT 8080

/**
 * TESTS:
 * #1 /ping
 * curl http://0.0.0.0:8080/ping
 * 
 * #2 /user
 * curl --header "Content-Type: application/json" \
        --request PUT \
        --data '{"name": "FooBar"}' \
        http://0.0.0.0:8080/user
 *
 * #3 /bad
 * curl -v http://0.0.0.0:8080/bad
 */

void route_ping(const headers_t* headers, const char* body);
void route_user(const headers_t* headers, const char* body);
void route_bad(const headers_t* headers, const char* body);
void killHandler(int sig);

HttpServer httpServer;


int main()
{
    setbuf(stdout, NULL);
    signal(SIGKILL, killHandler);
    signal(SIGTERM, killHandler);

    // Add some routes
    httpServer.addRoute("/ping", route_ping, "GET");
    httpServer.addRoute("/user", route_user, "PUT");
    httpServer.addRoute("/bad", route_bad);

    if (!httpServer.bind(HTTP_SERVER_PORT)) {
        exit(EXIT_FAILURE);
    }
    if (!httpServer.listen()) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}


void route_ping(const headers_t* headers, const char* body)
{
    char* response = "pong\n";

    httpServer.response(200, NULL, response);
}

void route_user(const headers_t* headerst, const char* body)
{
    printf("user request: %s\n", body);

    headers_t reqHeaders;

    // Create JSON header
    char** headers;
    headers = (char**)malloc(sizeof(char*) * 1);
    char* header = "Content-Type: application/json";
    headers[0] = header;
    reqHeaders.headers = headers;
    reqHeaders.count = 1;

    const char* response = "{\"id\": 20}";

    httpServer.response(200, &reqHeaders, response);
}

void route_bad(const headers_t* headers, const char* body)
{
    httpServer.response(400);
}


void killHandler(int sig)
{
    delete &httpServer;
}
