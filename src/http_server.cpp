#include "http_server.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>


HttpServer::HttpServer() {
    this->addrlen = sizeof(this->address);
}

HttpServer::~HttpServer() {
    if (this->fd > 0) {
        close(this->fd);
    }

    // Free
    for (uint i = 0; i < this->routesNumber; ++i) {
        if (this->routes[i]->method != NULL) {
            free(this->routes[i]->method);
        }
        free(this->routes[i]->path);
        free(this->routes[i]);
    }
    free(this->routes);
}

/**
 * Creates a socket and binds it to the given port.
 */
bool HttpServer::bind(uint port) {
    // Creating socket file descriptor
    if ((this->fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return false;
    }

    this->address.sin_family = AF_INET;
    this->address.sin_addr.s_addr = INADDR_ANY;
    this->address.sin_port = htons(port);

    memset(this->address.sin_zero, '\0', sizeof this->address.sin_zero);

    int bind = ::bind(this->fd, (struct sockaddr *)&this->address, sizeof(this->address));
    if (bind < 0) {
        perror("bind");
        return false;
    }

    return true;
}

/**
 * Listens to the previously binded port and forwards requests
 * to the processRequest method.
 */
bool HttpServer::listen() {
    if (::listen(this->fd, 10) < 0) {
        perror("listen");
        return false;
    }

    int socket;

    while (1) {
#if DEBUG
        printf("\n+++++++ Waiting for new connection ++++++++\n\n");
        fflush(stdout);
#endif

        socket = accept(this->fd, (struct sockaddr *)&this->address, (socklen_t*)&this->addrlen);
        if (socket < 0) {
            perror("accept");
            return false;
        }

        this->activeSocket = socket;

        this->processRequest();

        this->activeSocket = -1;
        close(socket);
    }
}

/**
 * Processes the incomming data, parses it into a request and route it.
 */
bool HttpServer::processRequest()
{
    char* buffer = (char*)malloc(sizeof(char) * HTTP_MAX_BUFFER_LEN);

    long valread = read(this->activeSocket, buffer, HTTP_MAX_BUFFER_LEN);
#if DEBUG
    printf("%.*s\n\n", valread, buffer);
#endif

    request_t request;
    headers_t headerst;
    char** body = (char**)malloc(sizeof(char*));

    if (!this->parseData(buffer, valread, &request, &headerst, body)) {
        return false;
    }

    int routeIdx = this->findRoute(&request);

    if (routeIdx != -1) {
        this->routes[routeIdx]->callback(&headerst, *body);
#if DEBUG
    } else {
        printf("no matching route\n");
#endif
    }

    // Free
    free(request.method);
    free(request.path);

    for (uint i = 0; i < headerst.count; ++i) {
        free(headerst.headers[i]);
    }
    free(headerst.headers);

    if (*body != NULL) {
        free(*body);
    }
    free(body);

    return true;
}

/**
 * Builds and sends a reponse.
 */
int HttpServer::response(
    uint code,
    const headers_t* headerst, /* = NULL */
    const char* body /* = NULL */
) {
    if (this->activeSocket == -1) {
        perror("no active connection");
        return -1;
    }

    const char* status = HttpServer::getStatus(code);

    char heading[48];
    sprintf(heading, "HTTP/1.1 %d %s", code, status);

    char headersBuff[4096];
    uint hdrPosTail = 0;
    this->appendHeader(headersBuff, heading, &hdrPosTail);

    if (headerst != NULL) {
        for (uint i = 0; i < headerst->count; ++i) {
            this->appendHeader(headersBuff, headerst->headers[i], &hdrPosTail);
        }
    }

    const int bodyLen = body == NULL ? 0 : strlen(body);

    if (bodyLen > 0) {
        char clHeader[32];
        sprintf(clHeader, "Content-Length: %d", bodyLen);

        this->appendHeader(headersBuff, clHeader, &hdrPosTail);
    }

    uint size = hdrPosTail + HttpServer::SEP_LEN + bodyLen;

    char* resp = (char*)malloc(sizeof(char) * size);
    memcpy(resp, headersBuff, hdrPosTail);

    if (bodyLen > 0) {
        memcpy(&resp[hdrPosTail], HttpServer::SEPARATOR, HttpServer::SEP_LEN);
        memcpy(&resp[hdrPosTail + HttpServer::SEP_LEN], body, bodyLen + 1); // \0
    }

    size_t written = write(this->activeSocket, resp, size);
    free(resp);

    return written;
}

/**
 * Parses the given request data (into structs + body).
 */
bool HttpServer::parseData(
    const char*const reqData,
    const long read,
    request_t* requestt,
    headers_t* headerst,
    char**const body
) {
    const char* headerBodySep = "\r\n\r\n";

    const char* pHdrsTail = strstr(reqData, headerBodySep);
    if (pHdrsTail == NULL) {
        perror("Missing headers tail");
        return false;
    }
    uint hdrsTailPos = pHdrsTail - &reqData[0];

    if (!this->parseStartLine(reqData, requestt)) {
        return false;
    }

    // Skip start-line
    const char* pHdrsBegin = strstr(reqData, HttpServer::SEPARATOR);
    if (pHdrsBegin != pHdrsTail) {
        uint beginPos = (pHdrsBegin + HttpServer::SEP_LEN) - &reqData[0];
        uint hdrsLen = (hdrsTailPos + HttpServer::SEP_LEN) - beginPos;

        char* headersRaw = (char*)malloc(hdrsLen + 1); // \0
        memcpy(headersRaw, &reqData[beginPos], hdrsLen);
        headersRaw[hdrsLen] = '\0';

        HttpServer::parseHeaders(headersRaw, headerst);
        free(headersRaw);
    }

    // We could also rely on the Content-Length header
    uint contentBeginPos = hdrsTailPos + strlen(headerBodySep);
    uint bodyLen = read - contentBeginPos;

    if (bodyLen > 0) {
        *body = (char*)malloc(bodyLen + 1); // \0
        memcpy(*body, &reqData[contentBeginPos], bodyLen);
        (*body)[bodyLen] = '\0';

#if DEBUG
        printf("body[%d]: %s\n", bodyLen, *body);
#endif
    } else {
        *body = NULL;
    }

    return true;
}

/**
 * Parses the start-line of the given request (into a request struct).
 */
bool HttpServer::parseStartLine(const char* reqData, request_t* requestt)
{
    // Method
    const char* pMethodTail = strstr(reqData, " ");
    if (pMethodTail == NULL) {
        perror("Invalid start-line (method)");
        return false;
    }

    uint methodSize = (pMethodTail - &reqData[0]);
    char* method = (char*)malloc(methodSize + 1); // \0
    memcpy(method, reqData, methodSize);
    method[methodSize] = '\0';

    requestt->method = method;

    // Path
    const char* pPathTail = strstr(pMethodTail + 1, " "); // space
    if (pPathTail == NULL) {
        perror("Invalid start-line (path)");
        return false;
    }

    uint pathSize = (pPathTail - (pMethodTail + 1));
    char* path = (char*)malloc(pathSize + 1); // \0
    memcpy(path, pMethodTail + 1, pathSize);
    path[pathSize] = '\0';

    requestt->path = path;

#if DEBUG
    printf("start-line:\n");
    printf("- method: %s\n", requestt->method);
    printf("- path: %s\n", requestt->path);
#endif

    return true;
}

/**
 * Parses the given (raw) headers (into a headers struct).
 */
bool HttpServer::parseHeaders(const char* headersRaw, headers_t* headerst)
{
    uint headersNumber = 0;

    char** headers = (char**)malloc(sizeof(char*) * HTTP_MAX_REQUEST_HEADERS);
    char* hdrRawTail;
    char* hdrRawBegin = (char*)headersRaw;

#if DEBUG
    printf("headers:\n");
#endif

    while ((hdrRawTail = strstr(hdrRawBegin, HttpServer::SEPARATOR)) != NULL) {
        uint size = hdrRawTail - hdrRawBegin;
        if (size == 0) {
            // End of headers
            break;
        }

        headers[headersNumber] = (char*)malloc(size + 1); // \0
        memcpy(headers[headersNumber], hdrRawBegin, size);

        headers[headersNumber][size] = '\0';

#if DEBUG
        printf("- %s\n", headers[headersNumber]);
#endif

        ++headersNumber;
        hdrRawBegin = hdrRawTail + HttpServer::SEP_LEN;
    }

    headerst->headers = headers;
    headerst->count = headersNumber;

    return true;
}

/**
 * Appends the given header into a buffer at a given position.
 */
void HttpServer::appendHeader(const char* buffer, const char* header, uint* pos)
{
    int len = strlen(header);
    memcpy((void*)&buffer[*pos], header, len);
    *pos += len;

    memcpy((void*)&buffer[*pos], HttpServer::SEPARATOR, HttpServer::SEP_LEN);
    *pos += HttpServer::SEP_LEN;
}

/**
 * Converts a status code into its status text.
 */
char* HttpServer::getStatus(const uint code)
{
    switch (code) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
    }

    return NULL;
}

/**
 * Adds the given route for matching upon request.
 */
bool HttpServer::addRoute(
    const char* path,
    std::function<void(const headers_t* headerst, const char* body)> callback,
    const char* method /* = NULL */
) {
    request_t requestt;
    requestt.method = (char*)method;
    requestt.path = (char*)path;

    int routeIdx = this->findRoute(&requestt);
    if (routeIdx != -1) {
        return false;
    }

    // Resize array
    route_t** routes;
    routes = (route_t**)malloc(sizeof(route_t*) * (this->routesNumber + 1));

    for (uint i = 0; i < this->routesNumber; ++i) {
        routes[i] = this->routes[i];
    }

    free(this->routes);
    this->routes = routes;

    route_t* newRoute = new route_t;

    char* pathCopy = (char*)malloc(strlen(path) + 1); // \0
    strcpy(pathCopy, path);
    newRoute->path = pathCopy;

    newRoute->callback = callback;

    if (method != NULL) {
        char* methodCopy = (char*)malloc(strlen(method) + 1); // \0
        strcpy(methodCopy, method);
        newRoute->method = methodCopy;
    }

    this->routes[this->routesNumber] = newRoute;

    ++this->routesNumber;

    return true;
}

/**
 * Finds the given route into the routes array.
 */
int HttpServer::findRoute(request_t* requestt)
{
    for (uint i = 0; i < this->routesNumber; ++i) {
        if (
            (
             this->routes[i]->method == NULL || requestt->method == NULL
             || strcmp(this->routes[i]->method, requestt->method) == 0
            )
            && strcmp(this->routes[i]->path, requestt->path) == 0
        ) {
            return i;
        }
    }

    return -1;
}
