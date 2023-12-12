#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 2048

enum Method {
    GET,
    HEAD,
    POST
};

typedef struct {
    char *key;
    char *value;
} header_field_t;
static header_field_t header_kv[17] = {{"\0","\0"}};

typedef struct {
    enum Method method;
    char *uri;
    char *protocol;
    char *host;
    char *user_agent;
    char *payload;
} Request;

char *get_header_field(char *key) {
    header_field_t *h = header_kv;
    while (h->key) {
        if (strcmp(h->key, "User-Agent") == 0) {
            return h->value;
        }
        h++;
    }
    return NULL;
}

int parse_request(char *raw_request, Request *request) {
    char *method = strtok(raw_request, " ");
    if (strcmp(method, "GET") == 0) {
        request->method = GET;
    } else if (strcmp(method, "HEAD") == 0) {
        request->method = HEAD;
    } else if (strcmp(method, "POST") == 0) {
        request->method = POST;
    } else {
        printf("Invalid HTTP method: %s\n", method);
        errno = -1;
        return -1;
    }

    request->uri = strtok(NULL, " ");
    request->protocol = strtok(NULL, " ");
    request->host = strtok(NULL, "\n\r");

    header_field_t *h = header_kv;
    char *t;
    while(h < header_kv + 16) {
        char *key, *val;
        key = strtok(NULL,"\r\n: \t");
        if (!key) {
            break;
        }

        val = strtok(NULL, "\r\n");
        while (*val && *val == ' ')
            val++;

        h->key = key;
        h->value = val;
        h++;
        fprintf(stderr, "[H] %s: %s\n", key, val);
        t = val + 1 + strlen(val);
        if (t[1] == '\r' && t[2] == '\n')
            break;
    }

    request->user_agent = get_header_field("User-Agent");
    if (request->method == POST) {
        request->payload = strtok(NULL,"\r\n");
    } else {
        request->payload = NULL;
    }

    return 0;
}

int main() {
    char buffer[BUFFER_SIZE];
    Request request;
    char resp[] = "HTTP/1.0 200 OK\r\n"
              "Server: webserver-c\r\n"
              "Content-type: text/html\r\n\r\n"
              "<html>hello, world</html>\r\n";

    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("webserver (socket)");
        return 1;
    }
    printf("socket created successfully\n");

    // Create the address to bind the socket to
    struct sockaddr_in host_addr;
    int host_addrlen = sizeof(host_addr);

    host_addr.sin_family = AF_INET;
    host_addr.sin_port = htons(PORT);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    struct sockaddr_in client_addr;
    int client_addrlen = sizeof(client_addr);

    // Bind the socket to the address
    if (bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0) {
        perror("webserver (bind)");
        return 1;
    }
    printf("socket successfully bound to address\n");

    // Listen for incoming connections
    if (listen(sockfd, SOMAXCONN) != 0) {
        perror("webserver (listen)");
        return 1;
    }
    printf("server listening for connections\n");

    for (;;) {
        // Accept incoming connections
        int newsockfd = accept(sockfd, (struct sockaddr *)&host_addr,
                               (socklen_t *)&host_addrlen);
        if (newsockfd < 0) {
            perror("webserver (accept)");
            continue;
        }
        printf("connection accepted\n");

        int sockn = getsockname(newsockfd, (struct sockaddr *)&client_addr,
                              (socklen_t *)&client_addrlen);

        if (sockn < 0) {
            perror("webserver (get client addr)");
            continue;
        }

        int valread = read(newsockfd, buffer, BUFFER_SIZE-1);
        buffer[valread] = '\0';
        if (valread < 0) {
            perror("webserver (read)");
            continue;
        }
        printf("read %d bytes from %s:%d: %s\n", valread, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        int parsed = parse_request(buffer, &request);
        if (parsed < 0) {
            perror("webserver (parse request)");
            continue;
        }
        printf("method: %d\n", request.method);
        printf("uri: %s\n", request.uri);
        printf("protocol: %s\n", request.protocol);
        printf("host: %s\n", request.host);
        printf("user-agent: %s\n", request.user_agent);
        printf("payload: %s\n", request.payload);

        int valwrite = write(newsockfd, resp, strlen(resp));
        if (valwrite < 0) {
            perror("webserver (write)");
            continue;
        }

        //sleep(1);
        close(newsockfd);
    }

    return 0;
}
