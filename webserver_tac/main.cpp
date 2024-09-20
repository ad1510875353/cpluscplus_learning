#include "code/server/webserver.h"
#include <unistd.h>

int main()
{
    WebServer server(8080, 3, 60000, true,
    3306,"root","123890","user",
    16,16,false,1,1024);
    server.Start();
}