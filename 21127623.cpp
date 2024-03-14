#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#define MAX_BUFFER_SIZE 4096

void extractURL(std::string URL, std::string &hostname,std::string&path){
    if (URL.substr(0, 7) == "http://") {
        URL = URL.substr(7);
    }
    size_t firstSlash = URL.find('/');
    if(firstSlash == std::string::npos){
        hostname = URL;
        path = '/';
    }
    else
    {
        hostname = URL.substr(0,firstSlash);
        path = URL.substr(firstSlash);
    }

}
std::string Get_Body(std::string response) {
    std::string body;

    //skip header
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        bodyStart += 4; 

        // check if has chunked
        size_t chunkedPos = response.find("Transfer-Encoding: chunked");
        if (chunkedPos != std::string::npos) {
            size_t chunkStart = bodyStart;//go to Body
            while (true) {
                // Find chunk size
                size_t nextline = response.find("\r\n", chunkStart);
                if (nextline == std::string::npos)
                    break; // End of file
                
                std::string chunkSizeStr = response.substr(chunkStart, nextline - chunkStart);

                size_t chunkSize;
                std::istringstream(chunkSizeStr) >> std::hex >> chunkSize;//convert hex to dec

                if (chunkSize == 0)
                    break;

                // Go to content chunkedsize\r\nContent
                chunkStart = nextline + 2;
                body += response.substr(chunkStart, chunkSize);
                //end of content\r\nchunksize
                chunkStart += chunkSize + 2; 
            }
        } else {
            body = response.substr(bodyStart);
        }
    }
    return body;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Wrong command" << std::endl;
        return 1;
    }

    //extract URL
    std::string URL = argv[1];
    std::string path,host;
    extractURL(URL,host,path);

    int port = 80;

    //GET request
    std::string request = "GET " + path +  " HTTP/1.1\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";

    //socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }

    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        std::cerr << "Failed to get host by name." << std::endl;
        close(sockfd);
        return 1;
    }

    struct sockaddr_in serverAddr;
    bzero((char*)&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    bcopy((char*)server->h_addr, (char*)&serverAddr.sin_addr.s_addr, server->h_length);
    serverAddr.sin_port = htons(port);

    // connect server
    if (connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to server." << std::endl;
        close(sockfd);
        return 1;
    }

     
    if (send(sockfd, request.c_str(), request.length(), 0) < 0) {
        std::cerr << "Failed to send request." << std::endl;
        close(sockfd);
        return 1;
    }

    // receive
    char buffer[MAX_BUFFER_SIZE];
    std::string response = "";
    int receive;
    while (true) {
        receive = recv(sockfd, buffer, sizeof(buffer), 0);
        if (receive < 0) {
            close(sockfd);
            return 1;
        } else if (receive == 0) {
            break;
        }
        response.append(buffer, receive);
    }
    
    //remove header and chunk
    std::string body = Get_Body(response);

    if(body.empty() == 0)
    {
        std::ofstream fOut;
        fOut.open(argv[2], std::ios::out);
        if(fOut.is_open())
        {
            fOut<<body;
            fOut.close();

            std::cout<<"Saved to "<<argv[2] <<std::endl;
        }
        else
        {
            std::cout<<"Cannot open file to write"<<std::endl;
        }
    }
    else
    {
        std::cerr<<"Error: No body found"<<std::endl;
    }

    close(sockfd);
    return 0;
}
