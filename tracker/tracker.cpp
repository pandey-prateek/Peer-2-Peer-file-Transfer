#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <pthread.h>
#include <semaphore.h>
#include <unordered_map>
sem_t x, y;
using namespace std;
struct tracker_info
{
    string serial_no;
    string tracker_file;
};
class Server_socket
{
public:
    unordered_map<string, unordered_map<string, string>> users;
    unordered_map<string, unordered_map<string, vector<string>>> groups;
    unordered_map<string, vector<string>> pending_requests;
    unordered_map<string, string> current_logged_in;
    unordered_map<string, unordered_map<string, vector<string>>> seeders;
    unordered_map<string, string> file_hash;
    unordered_map<string, string> chunks_hash;
    unordered_map<string, vector<string>> offsets;
    fstream fin;
    fstream fout;
    fstream file;
    fstream cred_save;

    int PORT;

    int general_socket_descriptor;
    int new_socket_descriptor;

    struct sockaddr_in address;
    int address_length;
    string filename;

    void create_socket()
    {
        if ((general_socket_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
            perror("[ERROR] : Socket failed");
            shutdown(new_socket_descriptor, 2);
            exit(EXIT_FAILURE);
        }
    }
    bool login(string username, string password)
    {
        /* fin.clear();
        fin.open("./login/login.txt", ios::in);
        std::string line;

        while (std::getline(fin, line))
        {
            vector<string> cred = split_string(line, ':');
            if (username == cred[0] && password == cred[1])
            {   session.username=username;
                session.password=password;
                return true;
            }
        }

        fin.close(); */
        if (users.find(username) != users.end() && users[username]["password"] == password)
        {
            return true;
        }
        return false;
    }
    bool create_user(string username, string password)
    {
        /* fout.clear();
        fout.open("./login/login.txt", ios::out | ios::app); */
        if (users.find(username) == users.end())
        {
            string s = username + ":" + password;
            unordered_map<string, string> map;
            map["username"] = username;
            map["password"] = password;
            users[username] = map;

            return true;
        }
        return false;
    }
    bool create_group(string groupId, string commandIp)
    {
        if (groups.find(groupId) == groups.end())
        {
            users[current_logged_in[commandIp]]["groupId"] = groupId;
            vector<string> v;
            v.push_back(current_logged_in[commandIp]);
            unordered_map<string, vector<string>> map;
            map["owner"] = v;
            groups[groupId] = map;
            return true;
        }
        return false;
    }
    bool join_group(string groupId, string commandIp)
    {
        if (groups.find(groupId) != groups.end())
        {
            if (pending_requests.find(groupId) == pending_requests.end())
            {
                vector<string> v;
                pending_requests[groupId] = v;
            }
            pending_requests[groupId].push_back(current_logged_in[commandIp]);

            return true;
        }
        return false;
    }
    bool accept_join_group(string groupId, string username, string commandIp)
    {
        if (groups.find(groupId) != groups.end() && groups[groupId]["owner"][0] == current_logged_in[commandIp])
        {
            if (pending_requests.find(groupId) == pending_requests.end())
            {
                return false;
            }
            for (auto it = pending_requests[groupId].begin(); it != pending_requests[groupId].end(); it++)
            {
                if ((*it) == username)
                {
                    pending_requests[groupId].erase(it);
                    break;
                }
            }

            if (groups[groupId].find("users") == groups[groupId].end())
            {
                vector<string> v;
                groups[groupId]["users"] = v;
            }

            groups[groupId]["users"].push_back(username);

            return true;
        }
        return false;
    }

    bool leave_group(string groupId, string commandIp)
    {
        if (groups.find(groupId) != groups.end())
        {
            for (auto it = groups[groupId]["users"].begin(); it != groups[groupId]["users"].end(); it++)
            {
                if ((*it) == current_logged_in[commandIp])
                {
                    groups[groupId]["users"].erase(it);

                    return true;
                }
            }
            for (auto it = groups[groupId]["owner"].begin(); it != groups[groupId]["owner"].end(); it++)
            {
                if ((*it) == current_logged_in[commandIp])
                {
                    groups[groupId]["owner"].erase(it);

                    return true;
                }
            }
        }

        return false;
    }
    string list_pending_req(string groupId)
    {
        string all_users;

        for (auto it = pending_requests[groupId].begin(); it != pending_requests[groupId].end(); it++)
        {
            all_users += (*it) + " ";
        }
        return all_users;
    }
    string getAllFiles(string groupId)
    {
        string all_files;

        for (auto it = groups[groupId]["files"].begin(); it != groups[groupId]["files"].end(); it++)
        {
            all_files += (*it) + " ";
        }
        return all_files;
    }
    string list_all_groups()
    {
        string all_group;

        for (auto i : groups)
        {
            all_group += i.first + " ";
        }
        return all_group;
    }
    bool check_login(string commandIp)
    {
        if (current_logged_in.find(commandIp) != current_logged_in.end() && !current_logged_in[commandIp].empty())
        {
            return true;
        }
        return false;
    }
    bool upload_file(string filename, string groupId, string f_hash, string chunk_hash, string offset, string commandIp)
    {
        if (groups.find(groupId) != groups.end())
        {
            if (seeders.find(filename) == seeders.end())
            {
                unordered_map<string, vector<string>> m;
                vector<string> v;
                v.push_back(commandIp);
                m["seeders"] = v;
                vector<string> grps;
                grps.push_back(groupId);
                m["groupId"] = grps;
                vector<string> hsh;
                hsh.push_back(f_hash);
                m["file_hash"] = hsh;
                vector<string> c_hsh;
                c_hsh.push_back(chunk_hash);
                m["chunk_hash"] = c_hsh;
                file_hash[filename] = f_hash;
                chunks_hash[filename] = chunk_hash;
                seeders[filename] = m;
                vector<string> off = split_string(offset, '|');
                offsets[filename] = off;
                if (groups[groupId].find("files") == groups[groupId].end())
                {
                    vector<string> v;
                    groups[groupId]["files"] = v;
                }
                groups[groupId]["files"].push_back(filename);
            }
            else
            {
                seeders[filename]["seeders"].push_back(commandIp);
                seeders[filename]["groupId"].push_back(groupId);
                seeders[filename]["file_hash"].push_back(f_hash);
                seeders[filename]["chunk_hash"].push_back(chunk_hash);
                bool found = false;
                for (int i = 0; i < groups[groupId]["files"].size(); i++)
                {
                    if (groups[groupId]["files"][i] == filename)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    groups[groupId]["files"].push_back(filename);
                file_hash[filename] = f_hash;
                chunks_hash[filename] = chunk_hash;
                vector<string> off = split_string(offset, '|');
                offsets[filename] = off;
            }
            return true;
        }
        return false;
    }
    bool stop_share(string filename, string groupId, string commandIp)
    {
        if (groups.find(groupId) != groups.end())
        {

            seeders.erase(filename);

            bool found = false;
            for (int i = 0; i < groups[groupId]["files"].size(); i++)
            {
                if (groups[groupId]["files"][i] == filename)
                {
                    groups[groupId]["files"].erase(groups[groupId]["files"].begin() + i);
                    break;
                }
            }

            file_hash.erase(filename);
            chunks_hash.erase(filename);
            offsets.erase(filename);

            return true;
        }
        return false;
    }
    bool check_user_exist(string groupId, string commandIp)
    {
        unordered_map<string, vector<string>> m = groups[groupId];

        for (auto it = groups[groupId]["users"].begin(); it != groups[groupId]["users"].end(); it++)
        {
            if ((*it) == current_logged_in[commandIp])
            {

                return true;
            }
        }
        for (auto it = groups[groupId]["owner"].begin(); it != groups[groupId]["owner"].end(); it++)
        {
            if ((*it) == current_logged_in[commandIp])
            {

                return true;
            }
        }
        return false;
    }
    bool check_file_exist_in_group(string filename, string groupId)
    {

        for (auto it = groups[groupId]["files"].begin(); it != groups[groupId]["files"].end(); it++)
        {
            if ((*it) == filename)
            {
                return true;
            }
        }
        return false;
    }
    string download_file(string filename, string groupId, string commandIp)
    {
        string seed_string;
        if (groups.find(groupId) != groups.end() && check_user_exist(groupId, commandIp) && check_file_exist_in_group(filename, groupId))
        {
            vector<string> seeds = seeders[filename]["seeders"];

            for (int i = 0; i < seeds.size(); i++)
            {
                seed_string += seeds[i] + "|";
            }
            string file_h = file_hash[filename];
            string chunk_h = chunks_hash[filename];
            seed_string += file_h + "|" + chunk_h + "|";

            vector<string> offset = offsets[filename];
            for (int i = 0; i < offset.size(); i++)
            {
                seed_string += offset[i] + ",";
            }
        }
        return seed_string.substr(0, seed_string.length() - 1);
    }

    void send_respones(string response, int new_socket_descriptor)
    {
        int bytes_sent;
        string msg = response;
        bytes_sent = write(new_socket_descriptor, msg.c_str(), msg.length());
        if (bytes_sent > 0)
        {
            cout << "[LOG] RESPONSE: " << msg << endl;
        }
        else
        {
            cout << "[LOG] ERROR WHILE SENDING RESPONSE" << endl;
        }
    }
    void getCommand(int new_socket_descriptor)
    {

        char buffer_ip[1024] = {};
        memset(buffer_ip, '\0', sizeof(buffer_ip));
        int bytes_receved = recv(new_socket_descriptor, buffer_ip, sizeof(buffer_ip), 0);
        if (bytes_receved == 0)
        {
            cout << "[LOG] CLIENT ERROR\n";
            pthread_exit(NULL);
            shutdown(new_socket_descriptor, 2);
            return;
        }
        string command_ip = buffer_ip;

        if (current_logged_in.find(command_ip) == current_logged_in.end())
        {
            current_logged_in[command_ip] = "";
        }

        /* while (1)
        { */
        string msg;
        char buffer[1024] = {};
        memset(buffer, '\0', sizeof(buffer));
        int valread = 0;
        cout << "[LOG] Awaiting for command from :" << command_ip << "\n";
        valread = recv(new_socket_descriptor, buffer, sizeof(buffer), 0);
        if (valread == 0)
        {
            cout << "[LOG] CLIENT ERROR";
            pthread_exit(NULL);
            shutdown(new_socket_descriptor, 2);
            return;
        }
        string command = buffer;
        vector<string> args = split_string(command, ' ');
        if (args[0] == "login")
        {
            if (login(args[1], args[2]))
            {
                current_logged_in[command_ip] = args[1];
                vector<string> args1 = split_string(command_ip, ':');
                users[args[1]]["address"] = args1[0];
                users[args[1]]["port"] = args1[1];
                send_respones("LOGIN SUCCESSFULL", new_socket_descriptor);
            }
            else
            {
                send_respones("LOGIN UNSUCCESSFULL", new_socket_descriptor);
            }
        }
        else if (args[0] == "create_user")
        {
            if (create_user(args[1], args[2]))
                send_respones("USER CREATED SUCCESSFULLY", new_socket_descriptor);
            else
                send_respones("USER ALREADY EXIST", new_socket_descriptor);
        }
        else if (args[0] == "create_group")
        {
            if (check_login(command_ip) && create_group(args[1], command_ip))
                send_respones("GROUP CREATED SUCCESSFULLY", new_socket_descriptor);
            else
                send_respones("ERROR WHILE CREATING GROUP", new_socket_descriptor);
        }
        else if (args[0] == "join_group")
        {
            if (check_login(command_ip) && join_group(args[1], command_ip))
            {
                send_respones("GROUP JOIN REQUEST PLACED SUCCESSFULLY", new_socket_descriptor);
            }
            else
            {
                send_respones("CANNOT PLACE JOIN REQUEST", new_socket_descriptor);
            }
        }
        else if (args[0] == "leave_group")
        {
            if (check_login(command_ip) && leave_group(args[1], command_ip))
            {
                send_respones("GROUP LEFT", new_socket_descriptor);
            }
            else
            {
                send_respones("ERROR WHILE FINDING GROUP", new_socket_descriptor);
            }
        }
        else if (args[0] == "list_requests")
        {
            if (check_login(command_ip))
                send_respones(list_pending_req(args[1]), new_socket_descriptor);
            else
                send_respones("CANNOT LIST REQUEST", new_socket_descriptor);
        }
        else if (args[0] == "accept_request")
        {
            if (check_login(command_ip) && accept_join_group(args[1], args[2], command_ip))
            {
                send_respones("JOIN REQUEST ACCEPTED", new_socket_descriptor);
            }
            else
            {
                send_respones("CANNOT ACCEPT JOIN REQUEST", new_socket_descriptor);
            }
        }
        else if (args[0] == "list_groups")
        {
            if (check_login(command_ip))
                send_respones(list_all_groups(), new_socket_descriptor);
            else
                send_respones("CANNOT LIST REQUEST", new_socket_descriptor);
        }
        else if (args[0] == "list_files")
        {
            if (check_login(command_ip)){
                string files=getAllFiles(args[1]);
                if(!files.empty()){
                    send_respones(files, new_socket_descriptor);
                }else{
                    send_respones("NO FILES IN GROUP", new_socket_descriptor);
                }
                
            }else
                send_respones("CANNOT LIST FILES OF GROUP", new_socket_descriptor);
        }
        else if (args[0] == "upload_file")
        {
            if (check_login(command_ip) && upload_file(args[1], args[2], args[3], args[4], args[5], command_ip))
            {
                send_respones("FILE UPLOADED SUCCESSFULLY", new_socket_descriptor);
            }
            else
            {
                send_respones("ERROR WHILE UPLOADING FILE", new_socket_descriptor);
            }
        }
        else if (args[0] == "download_file")
        {
            if (check_login(command_ip))
            {
                string seed_string = download_file(args[2], args[1], command_ip);
                if (!seed_string.empty())
                    send_respones(seed_string, new_socket_descriptor);
                else
                    send_respones("CANNOT START DOWNLOADING", new_socket_descriptor);
            }
            else
            {
                send_respones("ERROR WHILE DOWNLOADING FILE", new_socket_descriptor);
            }
        }
        else if (args[0] == "logout")
        {
            users[current_logged_in[command_ip]]["address"] = "";
            users[current_logged_in[command_ip]]["port"] = "";
            current_logged_in[command_ip] = "";

            send_respones("LOGOUT SUCCESSFULL", new_socket_descriptor);
        }
        else if (args[0] == "stop_share")
        {
            if (check_login(command_ip))
            {

                if (stop_share(args[2], args[1], command_ip))
                    send_respones("STOP SHARE EXECUTED SUCCESSFULLY", new_socket_descriptor);
                else
                    send_respones("STOP SHARE EXECUTION FAILED", new_socket_descriptor);
            }
            else
            {
                send_respones("PLEASE LOGIN", new_socket_descriptor);
            }
        }
        else if (args[0] == "quit")
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            send_respones("NOT VALID COMMAND", new_socket_descriptor);
        }
        /* } */
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
        if (listen(general_socket_descriptor, 3) < 0)
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
    void getfile_name()
    {

        char buffer[1024] = {};
        memset(buffer, '\0', sizeof(buffer));
        int valread;
        valread = recv(new_socket_descriptor, buffer, sizeof(buffer), 0);

        string s(buffer);
        if (s == "exit" || s.empty())
        {

            pthread_exit(NULL);
            shutdown(new_socket_descriptor, 2);
            return;
        }
        transmit_file(s);
    }
    void transmit_file(string filename)
    {

        file.open(filename, ios::in | ios::binary);
        if (!file.is_open())
        {

            shutdown(new_socket_descriptor, 2);
            exit(EXIT_FAILURE);
        }
        std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        int length = contents.length();
        int converted_number = htonl(length);

        write(new_socket_descriptor, &converted_number, sizeof(converted_number));
        int bytes_sent = 0;
        do
        {
            bytes_sent = write(new_socket_descriptor, contents.c_str(), contents.length());
            contents.clear();
        } while (bytes_sent > 0);
        bytes_sent = write(new_socket_descriptor, contents.c_str(), contents.length());
        file.close();
    }
};
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
    s->getCommand(socket_descriptor);
    close(socket_descriptor);
    pthread_exit(NULL);

    return NULL;
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
void *initialize_socket_connection(void *args)
{
    tracker_info *info = (tracker_info *)args;
    Server_socket s;
    vector<string> track = gettrackerInfo(info->tracker_file);
    for (int i = 0; i < track.size(); i++)
    {
        vector<string> args = split_string(track[i], ':');
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
            /* recive_data((void *)&serv); */
        }
        shutdown(s.new_socket_descriptor, 2);
    }
    pthread_exit(NULL);
    return NULL;
}
int main(int argc, char const *argv[])
{
    tracker_info *info = new tracker_info;
    info->serial_no = argv[2];
    info->tracker_file = argv[1];
    pthread_t server;
    pthread_create(&server, NULL, initialize_socket_connection, (void *)info);
    pthread_join(server, NULL);

    return 0;
}
