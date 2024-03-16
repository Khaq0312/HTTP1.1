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

//in google.com: "Transfer-Encoding: chunked"
//in bing.com:   "Transfer-Encoding:  chunked"

std::string Get_Body(std::string response) {
    std::string body;

    //skip header
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        bodyStart += 4; 

        // check if has chunked
        size_t chunkedPos = response.find("Transfer-Encoding:");
        if (chunkedPos != std::string::npos) {
            chunkedPos = response.find("chunked", chunkedPos + 19);//skip transfer encoding
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
                    {
                        break;
                    }

                    // Go to content chunkedsize\r\nContent
                    chunkStart = nextline + 2;
                    body += response.substr(chunkStart, chunkSize);
                    //end of content\r\nchunksize
                    chunkStart += chunkSize + 2; 
                }
            }
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
    std::string request = "GET " + path +  " HTTP/1.1\r\nHost: " + host + "\r\nConnection: keep-alive" +"\r\n\r\n";

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
    bool detect_CL = 0;//detect first content length in header, in case of unexpected content length in body
    std::string body = "";
    long long remaining_len = 0;
    bool isChunkedCase = 0;
    std::string temp = "";

    while (true) {
        receive = recv(sockfd, buffer, sizeof(buffer), 0);
        if (receive < 0) {
            close(sockfd);
            return 1;
        } else if (receive == 0) {
            break;
        }
        response.append(buffer, receive);
        //handle chunked
        size_t end_chunked = response.find("\r\n0\r\n\r\n");
        if (end_chunked != std::string::npos) {
            isChunkedCase = 1;
            break;
        }

        //handle content length
        if(detect_CL == 1 && remaining_len != 0){
            temp = std::string(buffer, receive);
            body += temp;
            remaining_len -= receive;
        }
        if(detect_CL == 1 && remaining_len == 0){
            break;
        }

        if(detect_CL == 0){
            size_t content_length = response.find("Content-Length:");
            if(content_length != std::string::npos)
            {
                detect_CL = 1;
                size_t content_size_pos = content_length + 16;
                
                while(response[content_size_pos] != '\r')
                {
                    temp += response[content_size_pos];
                    content_size_pos++;
                }
                size_t bodyStart = response.find("\r\n\r\n");
                bodyStart += 4;

                body = response.substr(bodyStart);
                remaining_len = stoll(temp);
                if(body.length() == remaining_len)
                {
                    break;
                }
                remaining_len-=body.length();
            }
        }
    }
    
    //case: chunked
    if(isChunkedCase)
    {
        body = Get_Body(response);
    }
    close(sockfd);

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
    return 0;
}
