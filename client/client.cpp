#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#define chunk_size 512000
sem_t x, y;
pthread_mutex_t write_mut;
using namespace std;
struct chunks_info
{
    string seeder;
    bool is_completed;
    bool is_available;
    bool is_busy;
    string offest;
    string filename;
    string destination;
    chunks_info()
    {
        is_completed = true;
        is_available = true;
        is_busy = false;
    }
};
class Client_socket
{
public:
    fstream file;
    string seeder;
    string offest;
    string destination;
    string filename;
    int PORT;

    int general_socket_descriptor;
    FILE *fp;
    struct sockaddr_in address;
    int address_length;

    void create_socket(bool *first)
    {
        if ((general_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("[ERROR] : Socket failed.\n");
            shutdown(general_socket_descriptor, 2);
            pthread_exit(NULL);
        }
        if (*first)
            cout << "[LOG] : Socket Created Successfully.\n";
    }

    bool create_connection()
    {
        if (connect(general_socket_descriptor, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            return false;
        }
        return true;
    }
    string calculate_hash(string s)
    {
        const unsigned char *chunk = reinterpret_cast<const unsigned char *>(s.c_str());
        unsigned char hash[SHA_DIGEST_LENGTH]; // == 20

        SHA1(chunk, s.length(), hash);
        string out;

        for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
        {

            std::stringstream sstream;
            sstream << std::hex << int(hash[i]);
            out += sstream.str();
        }
        return out;
    }

    void create_hash_of_chunk(string filename, string &request_string)
    {
        fstream file, fout;
        file.open(filename, ios::binary | ios::in);
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        int i = 0;
        string offsetstring;
        string chunk_hash;
        while (i < contents.length())
        {

            string chunk = contents.substr(i, chunk_size);

            offsetstring += to_string(i / chunk_size) + "|";

            i += chunk_size;
        }
        chunk_hash += calculate_hash(contents);
        request_string += " " + chunk_hash + " " + offsetstring.substr(0, offsetstring.length() - 1);
    }
    string create_hash_of_file(string filename)
    {
        fstream file, fout;
        file.open(filename, ios::binary | ios::in);
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        return calculate_hash(contents);
    }
    void send_host_address(string host_port, bool *sent)
    {
        string host_address = "127.0.0.1:";
        host_address += host_port;
        int bytes_sent = write(general_socket_descriptor, host_address.c_str(), host_address.length());
        if (bytes_sent <= 0)
        {
            shutdown(general_socket_descriptor, 2);
        }
        *sent = true;
    }
    vector<string> split_string(string s, char ch)
    {
        vector<string> tokens;

        stringstream ss(s);
        string token;

        while (getline(ss, token, ch))
        {
            tokens.push_back(token);
        }
        return tokens;
    }
    void receive_file(string filename)
    {

        file.open("./downloads/" + filename, ios::out | ios::trunc | ios::binary);
        if (file.is_open())
        {
            cout << "[LOG] : File Created.\n";
        }
        else
        {
            cout << "[ERROR] : File creation failed, Exititng.\n";
            shutdown(general_socket_descriptor, 2);
            pthread_exit(NULL);
            return;
        }
        char buffer[524288] = {};
        int received_int = 0;
        int length = 0;
        cout << "[LOG] : Waiting for size.\n";
        if (recv(general_socket_descriptor, &received_int, sizeof(received_int), 0) > 0)
        {
            length = ntohl(received_int);
            cout << "[LOG] : Length of chunk recceived " << length << "\n";
        }
        else
        {
            cout << "[ERROR] : recv failed, Exititng.\n";
            shutdown(general_socket_descriptor, 2);
            pthread_exit(NULL);
            return;
        }
        int valread = 0;
        int byte_received = 0;

        while (1)
        {

            byte_received = recv(general_socket_descriptor, buffer, sizeof(buffer), 0);
            valread += byte_received;

            cout << "[LOG] : Data received " << byte_received << " bytes\n";
            cout << "[LOG] : Saving data to file.\n";
            for (int i = 0; i < byte_received; i++)
                file.put(buffer[i]);
            memset(buffer, 0, sizeof(buffer));
            if (valread >= length || byte_received == 0)
            {
                break;
            }
        }
        // pthread_mutex_unlock(&myMutex);
        file.close();
        cout << "[LOG] : File Saved.\n";

        cout << "[LOG] : File Transfer Complete.\n";
    }
    void send_command(bool *quit)
    {
        while (1)
        {
            string s;
            cout << "< " << flush;
            getline(cin, s);
            vector<string> args = split_string(s, ' ');
            if (args[0] == "login" || args[0] == "create_user" || args[0] == "accept_request" || args[0] == "stop_share")
            {
                if (args.size() != 3)
                {
                    cout << "< INVALID INPUT" << endl;
                    continue;
                }
            }
            else if (args[0] == "create_group" || args[0] == "join_group" || args[0] == "leave_group" || args[0] == "list_requests" || args[0] == "list_files")
            {
                if (args.size() != 2)
                {
                    cout << "< INVALID INPUT" << endl;
                    continue;
                }
            }
            else if (args[0] == "list_groups" || args[0] == "quit")
            {
                if (args.size() != 1)
                {
                    cout << "< INVALID INPUT" << endl;
                    continue;
                }
            }
            else if (args[0] == "upload_file")
            {
                if (args.size() != 3)
                {
                    cout << "< INVALID INPUT" << endl;
                    continue;
                }
                /* vector<string> filename=split_string(args[1],'/');
                s=args[0]+filename[filename.size()-1]+args[2]; */
                s += " " + create_hash_of_file(args[1]);
                create_hash_of_chunk(args[1], s);
            }
            else if (args[0] == "download_file")
            {
                if (args.size() != 4)
                {
                    cout << "< INVALID INPUT" << endl;
                    continue;
                }
                int bytes_sent = write(general_socket_descriptor, s.c_str(), s.length());
                if (bytes_sent <= 0 || s == "exit")
                {
                    pthread_exit(NULL);
                    shutdown(general_socket_descriptor, 2);
                    return;
                }
                char res_buffer[2048] = {};
                memset(res_buffer, '\0', sizeof(res_buffer));
                int valread;
                valread = recv(general_socket_descriptor, res_buffer, sizeof(res_buffer), 0);
                if (valread == 0)
                {
                    cout << "SERVER ERROR" << endl;
                    close(general_socket_descriptor);
                }
                string response(res_buffer);
                vector<string> response_args = split_string(response, '|');
                connect_and_download_from_seeders(response_args, args[2], args[3]);
                break;
            }

            int bytes_sent = write(general_socket_descriptor, s.c_str(), s.length());
            if (bytes_sent <= 0 || s == "exit")
            {
                pthread_exit(NULL);
                shutdown(general_socket_descriptor, 2);
                return;
            }
            if (args[0] == "quit")
            {
                *quit = true;
                return;
            }
            char res_buffer[1024] = {};
            memset(res_buffer, '\0', sizeof(res_buffer));
            int valread;
            valread = recv(general_socket_descriptor, res_buffer, sizeof(res_buffer), 0);
            if (valread == 0)
            {
                cout << "SERVER ERROR" << endl;
                close(general_socket_descriptor);
            }
            cout << "SERVER RESPONSE : " << res_buffer << endl;
            break;
            /* receive_file(s); */
        }
    }
    static void *middle(void *args)
    {
        ((Client_socket *)args)->create_connection_with_seeders_and_download(((Client_socket *)args)->filename, ((Client_socket *)args)->offest, ((Client_socket *)args)->seeder, ((Client_socket *)args)->destination);
        return NULL;
    }
    void create_connection_with_seeders_and_download(string filename, string offest, string seeder, string destination)
    {

        int socket_descriptor;
        Client_socket c;

        vector<string> track_det = c.split_string(seeder, ':');

        if ((socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        {
            perror("[ERROR] : Socket failed.\n");
            shutdown(socket_descriptor, 2);
            pthread_exit(NULL);
        }
        struct sockaddr_in address;

        address.sin_family = AF_INET;
        address.sin_port = htons(stoi(track_det[1]));
        int address_length = sizeof(address);
        if (inet_pton(AF_INET, track_det[0].c_str(), &(address.sin_addr)) <= 0)
        {
            cout << "[ERROR] : Invalid address\n";
            shutdown(socket_descriptor, 2);
            pthread_exit(NULL);
        }

        if (connect(socket_descriptor, (struct sockaddr *)&address, sizeof(address)) >= 0)
        {
            string msg = filename + "|" + offest;

            int bytes_sent = write(socket_descriptor, msg.c_str(), msg.length());
            if (bytes_sent <= 0)
            {
                shutdown(socket_descriptor, 2);
                pthread_exit(NULL);
            }

            char buffer[512000] = {};
            int received_int = 0;
            int length = 0;

            if (recv(socket_descriptor, &received_int, sizeof(received_int), 0) > 0)
            {
                length = ntohl(received_int);
                cout << "[LOG] : Length of chunk recceived " << length << "\n";
            }
            else
            {
                cout << "[ERROR] : recv failed, Exititng.\n";
                shutdown(socket_descriptor, 2);
                pthread_exit(NULL);
            }

            int valread = 0;
            int byte_received = 0;

            string chunk;
            string path = destination + "/" + filename;
            fstream file;
            file.open(path,ios::out|ios::app | ios::binary);
            file.seekp(stoi(offest)*chunk_size, ios::beg);
            while (1)
            {

                byte_received = recv(socket_descriptor, buffer, sizeof(buffer), 0);
                valread += byte_received;
                
                
                memset(buffer, 0, sizeof(buffer));
                for (int i = 0; i < byte_received; i++)
                    file.put(buffer[i]);
                
                if (valread >= chunk_size || byte_received == 0)
                {
                    break;
                }
            }
           /*  
            
            if (file.is_open())
            {
                
                
                
            } */
           
            file.close();
           /*  int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
            pwrite(fd, chunk.c_str(), chunk.length(), stoi(offest) * chunk_size);
            close(fd); */
            shutdown(socket_descriptor, 2);
        }
        else
        {
            cout << "[ERROR] : Invalid address\n";
            pthread_exit(NULL);
            shutdown(socket_descriptor, 2);
        }
    }
    void connect_and_download_from_seeders(vector<string> response_args, string filename, string destination)
    {
        Client_socket c;
        vector<string> seeders;
        for (int i = 0; i < response_args.size() - 3; i++)
        {
            seeders.push_back(response_args[i]);
        }
        vector<string> offsets = split_string(response_args[response_args.size() - 1], ',');
        pthread_t threads[seeders.size()];

        int i = 0;
        while (i < offsets.size())
        {
            Client_socket c;

            c.offest = offsets[i];

            c.seeder = seeders[i % seeders.size()];

            c.filename = filename;
            c.destination = destination;
            pthread_t thread;

            create_connection_with_seeders_and_download(filename, offsets[i], seeders[i % seeders.size()], destination);
            // pthread_create(&thread, NULL, Client_socket::middle, (void *)&c);

            i++;
        }
    }
};

class Server_socket
{
public:
    fstream file;
    fstream flogger;

    int PORT;

    int general_socket_descriptor;
    int new_socket_descriptor;

    struct sockaddr_in address;
    int address_length;
    string filename;
    Server_socket()
    {
        flogger.open("logs.txt", ios::out | ios::app);
    }
    void create_socket()
    {
        if ((general_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            perror("[ERROR] : Socket failed");
            close(new_socket_descriptor);
            exit(EXIT_FAILURE);
        }
    }

    void bind_socket()
    {
        if (bind(general_socket_descriptor, (struct sockaddr *)&address, sizeof(address)) < 0)
        {
            perror("[ERROR] : Bind failed");
            exit(EXIT_FAILURE);
        }
    }

    void set_listen_set()
    {
        if (listen(general_socket_descriptor, 10) < 0)
        {
            perror("[ERROR] : Listen");
            shutdown(new_socket_descriptor, 2);
            exit(EXIT_FAILURE);
        }
    }

    int accept_connection()
    {
        int new_socket_descriptor;
        if ((new_socket_descriptor = accept(general_socket_descriptor, (struct sockaddr *)&address, (socklen_t *)&address_length)) < 0)
        {
            perror("[ERROR] : Accept");
            shutdown(new_socket_descriptor, 2);
            exit(EXIT_FAILURE);
        }
        return new_socket_descriptor;
    }
    vector<string> split_string(string s, char ch)
    {
        vector<string> tokens;

        stringstream ss(s);
        string token;

        while (getline(ss, token, ch))
        {
            tokens.push_back(token);
        }
        return tokens;
    }
    void transmit_file(int new_socket_descriptor)
    {
        char bufferr[1024] = {};
        memset(bufferr, '\0', sizeof(bufferr));
        int valread;

        valread = recv(new_socket_descriptor, bufferr, sizeof(bufferr), 0);
        string s(bufferr);
        vector<string> v = split_string(s, '|');

        if (s == "exit" || s.empty())
        {
            flogger << "[LOG] : Exitting : \n";

            shutdown(new_socket_descriptor, 2);
            pthread_exit(NULL);
            return;
        }
        fstream file;
        file.open(v[0], ios::in | ios::binary);

        if (!file.is_open())
        {
            shutdown(new_socket_descriptor, 2);
            exit(EXIT_FAILURE);
        }
      
        /* int fd = open(v[0].c_str(), O_RDONLY);
        char buffer[chunk_size] = {};

        pread(fd, buffer, chunk_size, stoi(v[1]) * chunk_size); */
        /* string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        int fsize = lseek(fd, 0, SEEK_END);
        int remaining = fsize - stoi(v[1]) * chunk_size;
        int chunk = min(512000, remaining);
        int converted_number = htonl(chunk); */


       
        /* flogger << "[LOG] : Sending...\n";
        int bytes_sent = 0;
        do
        {
            bytes_sent = write(new_socket_descriptor, chunk.c_str(), chunk.length());

            chunk.clear();
        } while (bytes_sent > 0); */


     /*    int bytes_sent = write(new_socket_descriptor, buffer, chunk); */
/* 
        file.open(v[0], ios::in | ios::binary);
        if (!file.is_open())
        {

            shutdown(new_socket_descriptor, 2);
            exit(EXIT_FAILURE);
        } */
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        string chunk = contents.substr(stoi(v[1]) * chunk_size,chunk_size);

        int length = chunk.length();
        int converted_number = htonl(length);

        write(new_socket_descriptor, &converted_number, sizeof(converted_number));
        int bytes_sent = 0;
        /* do
        {
            bytes_sent = write(new_socket_descriptor, chunk.c_str(), chunk.length());
            chunk.clear();
        } while (bytes_sent > 0); */
        bytes_sent = write(new_socket_descriptor, chunk.c_str(), chunk.length());
        file.close();
    }
};
struct tracker_info
{
    string user_info;
    string tracker_file;
};
vector<string> split_string(string s, char ch)
{
    vector<string> tokens;

    stringstream ss(s);
    string token;

    while (getline(ss, token, ch))
    {
        tokens.push_back(token);
    }
    return tokens;
}
vector<string> gettrackerInfo(string filename)
{
    fstream fin;
    vector<string> v;
    fin.open(filename, ios::in);
    std::string line;

    while (std::getline(fin, line))
    {
        v.push_back(line);
    }
    return v;
}

void *initialize_client_connection(void *args)
{

    tracker_info *info = (tracker_info *)args;
    string host_info = info->user_info;
    vector<string> ip = split_string(host_info, ':');

    vector<string> track = gettrackerInfo(info->tracker_file);

    bool sent = false;
    string tracker_ip, tracker_port;
    bool first = true;
    bool connect;

    int last_track = -1;
label:
    bool quit = false;
    for (int i = 0; i < track.size(); i++)
    {
        if (last_track != -1)
        {
            i = last_track + 1;
        }
        vector<string> track_det = split_string(track[i], ':');
        Client_socket c;
        c.create_socket(&first);
        first = false;
        c.PORT = stoi(track_det[1]);

        c.address.sin_family = AF_INET;
        c.address.sin_port = htons(c.PORT);
        c.address_length = sizeof(c.address);
        if (inet_pton(AF_INET, track_det[0].c_str(), &(c.address.sin_addr)) <= 0)
        {
            cout << "[ERROR] : Invalid address\n";
        }

        if (connect = c.create_connection())
        {
            last_track = i;
            tracker_ip = track_det[0];
            tracker_port = track_det[1];
            cout << "[LOG] : CONNECTED TO TRACAKER " << i << endl;
            c.send_host_address(ip[1], &sent);
            c.send_command(&quit);
            if (quit)
            {
                shutdown(c.general_socket_descriptor, 2);
                continue;
            }

            shutdown(c.general_socket_descriptor, 2);
            break;
        }
        cout << "[LOG] : TRACAKER " << i << " NOT RESPONDING" << endl;
        shutdown(c.general_socket_descriptor, 2);
    }

    while (connect)
    {
        Client_socket c;
        c.create_socket(&first);
        c.PORT = stoi(tracker_port);

        c.address.sin_family = AF_INET;
        c.address.sin_port = htons(c.PORT);
        c.address_length = sizeof(c.address);
        if (inet_pton(AF_INET, tracker_ip.c_str(), &(c.address.sin_addr)) <= 0)
        {
            cout << "[ERROR] : Invalid address" << endl;
        }

        c.create_connection();

        c.send_host_address(ip[1], &sent);
        c.send_command(&quit);
        shutdown(c.general_socket_descriptor, 2);
        if (quit)
        {
            goto label;
            break;
        }
    }
    return NULL;
}

struct server_det
{
    Server_socket *s;
    int socket_descriptor;
};
void *recive_data(void *args)
{
    server_det *server = (server_det *)args;
    Server_socket *s = server->s;
    int socket_descriptor = server->socket_descriptor;
    s->transmit_file(socket_descriptor);
    close(socket_descriptor);
    pthread_exit(NULL);

    return NULL;
}

void *initialize_socket_connection(void *ip_port)
{
    Server_socket s;
    vector<string> args = split_string((char *)ip_port, ':');
    s.PORT = stoi(args[1]);
    s.create_socket();
    s.address.sin_family = AF_INET;
    s.address.sin_addr.s_addr = INADDR_ANY;
    s.address.sin_port = htons(s.PORT);
    s.address_length = sizeof(s.address);

    s.bind_socket();
    s.set_listen_set();
    while (1)
    {
        server_det serv;
        serv.s = &s;
        serv.socket_descriptor = s.accept_connection();
        pthread_t recive;
        pthread_create(&recive, NULL, recive_data, (void *)&serv);
        pthread_detach(recive);
    }
    pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
    tracker_info *info = new tracker_info;
    info->user_info = argv[1];
    info->tracker_file = argv[2];
    // initialize_socket_connection((void *)"127.0.0.1:8080");
    pthread_t server;
    pthread_create(&server, NULL, initialize_socket_connection, (void *)argv[1]);
    pthread_t client;
    initialize_client_connection((void *)info);

    return 0;
}
