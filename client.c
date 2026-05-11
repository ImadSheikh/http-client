#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_URL 2048
#define MAX_REQ 4096
#define MAX_RES 100000
#define MAX_PARAMS 1024

// ---------------- USAGE ----------------

void usage()
{
    printf("Usage: client [-r n < pr1=value1 pr2=value2 …>] <URL>\n");
    exit(1);
}

// ---------------- SAFE STR DUP ----------------

char *safe_strdup(const char *s)
{
    size_t len = strlen(s);
    char *p = malloc(len + 1);
    if (!p)
    {
        perror("malloc");
        exit(1);
    }
    memcpy(p, s, len + 1);
    return p;
}

// ---------------- URL PARSER ----------------

void parse_url(const char *url, char *host, int *port, char *path)
{
    if (strncmp(url, "http://", 7) != 0)
        usage();

    url += 7;

    const char *slash = strchr(url, '/');
    const char *colon = strchr(url, ':');

    if (!slash)
        slash = url + strlen(url);

    // host
    if (colon && colon < slash)
    {
        strncpy(host, url, colon - url);
        host[colon - url] = '\0';

        *port = atoi(colon + 1);
        if (*port <= 0 || *port >= 65536)
            usage();
    }
    else
    {
        *port = 80;
        strncpy(host, url, slash - url);
        host[slash - url] = '\0';
    }

    // path
    if (*slash)
        strcpy(path, slash);
    else
        strcpy(path, "/");
}

// ---------------- QUERY BUILDER ----------------

void build_path_with_query(char *out, const char *path, const char *params)
{
    strcpy(out, path);

    if (params && strlen(params) > 0)
    {
        strcat(out, "?");
        strcat(out, params);
    }
}

// ---------------- REQUEST BUILDER ----------------

void build_request(char *req, const char *host, const char *path)
{
    sprintf(req,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            path, host);
}

// ---------------- CONNECT ----------------

int connect_to_host(const char *host, int port)
{
    struct hostent *server = gethostbyname(host);
    if (!server)
    {
        herror("gethostbyname");
        exit(1);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(1);
    }

    return sock;
}

// ---------------- RECEIVE ----------------

int receive_response(int sock, char **out)
{
    char buffer[1024];
    char *res = NULL;
    int total = 0, n;

    while ((n = read(sock, buffer, sizeof(buffer))) > 0)
    {
        char *tmp = realloc(res, total + n + 1);
        if (!tmp)
        {
            free(res);
            perror("realloc");
            exit(1);
        }

        res = tmp;
        memcpy(res + total, buffer, n);
        total += n;
    }

    if (n < 0)
    {
        perror("read");
        exit(1);
    }

    if (res)
        res[total] = '\0';

    *out = res;
    return total;
}

// ---------------- LOCATION PARSE ----------------

int extract_location(const char *response, char *location)
{
    char *loc = strstr(response, "Location:");
    if (!loc)
        return 0;

    loc += 9;
    while (*loc == ' ')
        loc++;

    char *end = strstr(loc, "\r\n");
    if (!end)
        return 0;

    strncpy(location, loc, end - loc);
    location[end - loc] = '\0';

    return 1;
}

// ---------------- HTTP EXECUTION ----------------

void http_run(char *url, char *params)
{
    char host[256], path[1024], full_path[2048];
    int port;

    while (1)
    {

        parse_url(url, host, &port, path);
        build_path_with_query(full_path, path, params);

        char request[MAX_REQ];
        build_request(request, host, full_path);

        printf("HTTP request =\n%s\nLEN = %ld\n", request, strlen(request));

        int sock = connect_to_host(host, port);

        if (write(sock, request, strlen(request)) < 0)
        {
            perror("write");
            exit(1);
        }

        char *response = NULL;
        int size = receive_response(sock, &response);

        close(sock);

        if (!response)
        {
            printf("\n Total received response bytes: 0\n");
            break;
        }

        printf("%s\n", response);
        printf("\n Total received response bytes: %d\n", size);

        // ---------------- STATUS CODE ----------------
        int status = 0;
        char *http = strstr(response, "HTTP/");
        if (http)
            sscanf(http, "HTTP/%*s %d", &status);

        // ---------------- REDIRECT ----------------
        if (status >= 300 && status < 400)
        {
            char new_url[2048];

            if (extract_location(response, new_url) &&
                strncmp(new_url, "http://", 7) == 0)
            {

                free(response);
                free(url); // FIX LEAK
                url = safe_strdup(new_url);
                continue;
            }
        }

        free(response);
        break;
    }
}

// ---------------- CLI PARSER ----------------

void parse_cli(int argc, char *argv[], char *url, char *params)
{
    int i = 1;
    url[0] = '\0';
    params[0] = '\0';

    if (argc < 2)
        usage();

    while (i < argc)
    {

        if (strcmp(argv[i], "-r") == 0)
        {
            i++;
            if (i >= argc || !isdigit(argv[i][0]))
                usage();

            int n = atoi(argv[i]);
            i++;

            for (int j = 0; j < n; j++)
            {
                if (i >= argc || !strchr(argv[i], '='))
                    usage();

                if (strlen(params) > 0)
                    strcat(params, "&");

                strcat(params, argv[i]);
                i++;
            }
        }
        else if (strncmp(argv[i], "http://", 7) == 0)
        {
            strcpy(url, argv[i]);
            i++;
        }
        else
        {
            usage();
        }
    }

    if (strlen(url) == 0)
        usage();
}

// ---------------- MAIN ----------------

int main(int argc, char *argv[])
{
    char url[MAX_URL];
    char params[MAX_PARAMS];

    parse_cli(argc, argv, url, params);
    http_run(url, params);

    return 0;
}