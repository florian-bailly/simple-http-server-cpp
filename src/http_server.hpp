#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <functional>

#define HTTP_MAX_BUFFER_LEN 30000
#define HTTP_MAX_REQUEST_HEADERS 128


struct request_t {
    char* method;
    char* path;
};

/**
 * headers: Array of plain headers (name: value).
 */
struct headers_t {
    char** headers;
    uint count = 0;
};

/**
 * callback: Function to call when the path matches.
 * method: If not specified matches all methods.
 */
struct route_t {
    char* path;
    std::function<void(headers_t* headerst, const char* body)> callback;
    char* method = NULL;
};


class HttpServer
{
private:
    // File descriptor for the bind
    int fd = 0;

    sockaddr_in address;
    uint addrlen;

    // Active socket when accepting connection
    int activeSocket;

    route_t** routes;
    uint routesNumber = 0;

    const char* SEPARATOR = "\r\n";
    const int SEP_LEN = 2;


    bool processRequest();

    bool parseData(
        const char*const reqData,
        const long read,
        request_t* requestt,
        headers_t* headerst,
        char**const body
    );

    bool parseStartLine(const char* reqData, request_t* requestt);

    bool parseHeaders(const char* headersRaw, headers_t* headerst);

    void appendHeader(const char* buffer, const char* header, uint* pos);

    char* getStatus(const uint code);

public:

    const char* METHOD_GET = "GET";
    const char* METHOD_POST = "POST";
    const char* METHOD_PUT = "PUT";
    const char* METHOD_DELETE = "DELETE";


    HttpServer();
    ~HttpServer();

    bool bind(uint port);

    bool listen();

    int response(uint code, const headers_t* headerst = NULL, const char* body = NULL);

    bool addRoute(
        const char* route,
        std::function<void(const headers_t* headerst, const char* body)> callback,
        const char* method = NULL
    );

    int findRoute(request_t* requestt);

};

#endif
