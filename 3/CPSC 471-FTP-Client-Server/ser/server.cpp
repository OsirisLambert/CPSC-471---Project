#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>  /* Contains definitions of data types used in system calls */
#include <sys/socket.h> /* Includes definitions of structures needed for sockets */
#include <netinet/in.h> /* Contains constants and structures needed for internet domain addresses. */
#include <netdb.h>
#include <arpa/inet.h> // inet_pton
#include <unistd.h>     /* Contains standard unix functions */
#include <errno.h>
#include <sys/wait.h>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <sys/stat.h>
#include <dirent.h>
#include <sstream>

using namespace std;

char *CONTROL_PORT;
char *DATA_PORT;
string remBuf;	//used in function readWholeLine() as a global variable

//get rid of space
// trim from start
static inline std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
	return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}

// bind and listen
int server_listen(const char *port) {
	// Create address structs
	struct addrinfo hints, *res;
	int sock_fd;

	/* Set the structure to all zeros */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int addr_check = getaddrinfo(NULL, port, &hints, &res);
	if (addr_check != 0)
	{
		fprintf(stderr, "Cannot get info\n");
		return -1;
	}

	//connect through loop
	struct addrinfo *i;
	for (i = res; i != NULL; i = i->ai_next)
	{
		// Create the socket
		sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock_fd < 0)
		{
			perror("socket open");
			continue;
		}

		// Set socket options
		int yes = 1;
		int set_check = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (set_check == -1){
			perror("setsockopt");
			exit(1);
		}

		// Bind
		int bind_check = bind(sock_fd, res->ai_addr, res->ai_addrlen);
		if (bind_check != 0){
			close(sock_fd);
			perror("socket bind");
			continue;
		}

		break;
	}

	// No binds happened
	if (i == NULL){
		perror("bind FAILED\n");
		exit(1);
	}

	// free address information
	freeaddrinfo(res);

//listen
	if (listen(sock_fd, 100) < 0) {
		perror("listen");
		exit(1);
	}

	return sock_fd;
}

//bind
int bindsocket(const char *port) {
	// Create address structs
	struct addrinfo hints, *res;
	int sock_fd;
	// Load up address structs
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int addr_check = getaddrinfo(NULL, port, &hints, &res);
	if (addr_check != 0)
	{
		fprintf(stderr, "Cannot get info\n");
		return -1;
	}

	//connect through loop
	struct addrinfo *i;
	for (i = res; i != NULL; i = i->ai_next)
	{
		// Create the socket
		sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock_fd < 0)
		{
			perror("socket open");
			continue;
		}

		// Set socket options
		int yes = 1;
		int set_check = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (set_check == -1){
			perror("setsockopt");
			exit(1);
		}

		// Bind
		int bind_check = bind(sock_fd, res->ai_addr, res->ai_addrlen);
		if (bind_check != 0){
			close(sock_fd);
			perror("socket bind");
			continue;
		}

		break;
	}

	// No binds happened
	if (i == NULL){
		perror("bind FAILED\n");
		exit(1);
	}

	// free address information
	freeaddrinfo(res);

	return sock_fd;
}

//IPv4 - IPv6
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// accept connection of a client
int accept_connection(int server_fd) {
	// connector's address information
	struct sockaddr_storage connector_addr; 
	char s[INET6_ADDRSTRLEN];
	socklen_t sin_size = sizeof connector_addr;

	// Accept connections
	int client_fd = accept(server_fd, (struct sockaddr *)&connector_addr, &sin_size);
	if (client_fd == -1)
	{
		perror("accept");
		return -1;
	}

	// Print out IP address
	inet_ntop(connector_addr.ss_family, get_in_addr((struct sockaddr *)&connector_addr), s, sizeof s);
	printf("server: got connection from %s\n", s);
	// Setting Timeout
	struct timeval tv;
	tv.tv_sec = 200;  /* 200 Seconds Timeout */
	tv.tv_usec = 0;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));
	return client_fd;
}

// connects to host
int make_client_connection(const char *host, const char *port)
{
	// Create address structs
	struct addrinfo hints, *res;
	int sock_fd;

	// Load up address structs
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;


	int addr_check = getaddrinfo(host, port, &hints, &res);
	if (addr_check != 0)
	{
		fprintf(stderr, "Cannot get address info\n");
		return -1;
	}

	// Loop through results, connect to first one we can
	struct addrinfo *i;
	for (i = res; i != NULL; i = i->ai_next)
	{
		// Create the socket
		sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock_fd < 0)
		{
			perror("open socket");
			continue;
		}

		// Make connection
		int connect_status = connect(sock_fd, i->ai_addr, i->ai_addrlen);
		if (connect_status < 0)
		{
			close(sock_fd);
			perror("connect");
			continue;
		}

		
		break;
	}

	// No binds happened
	if (i == NULL)
	{
		perror("connection");
		exit(1);
	}

	// Print out IP address
	char s[INET6_ADDRSTRLEN];
	inet_ntop(i->ai_family, get_in_addr((struct sockaddr *)i->ai_addr), s, sizeof s);


	// Don't need the structure with address info any more
	freeaddrinfo(res);
	return sock_fd;
}


//connect to host using sock_fd
int make_client_connection_with_sockfd(int sock_fd, const char *host, const char *port)
{
	struct addrinfo hints, *res;

	// first, load up address structs with getaddrinfo():

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	getaddrinfo(host, port, &hints, &res);
	// connect!
	int stat = connect(sock_fd, res->ai_addr, res->ai_addrlen);
	if (stat == -1) {
		cout << "Connect Error " << strerror(errno) << endl;
		return -1;
	}

	return sock_fd;
}

//function to send all data in buffer
int send_all(int socket, const void *buffer, size_t length) {
	size_t i = 0;
	while (i < length){
		int sentSize = send(socket, buffer, length - i, MSG_NOSIGNAL);
		if (sentSize < 0) {
			return errno;
		}
		else {
			i += sentSize;
		}
	}
	return 0;
}

//keep receiving until "\r\n"
int recv_all(int serverfd, string& result) {
	int len = 0;
	while (true) {
		char buf[10001];
		int readSize;
		if ((readSize = recv(serverfd, buf, 10000, 0)) > 0) {
			result = string(buf, buf + readSize);
			len += readSize;
		}
		else if (readSize < 0) {
			return -1;
		}
		else {
			return len;
		}
	}
}

// Receives data, change to binary, write it into fd
int recv_all_binary(int serverfd, FILE *fd) {
	unsigned char buf[10001];
	int readSize = 0;
	int len = 0;
	while ((readSize = recv(serverfd, buf, 10000, 0)) > 0) {
		len += readSize;
		fwrite(buf, 1, readSize, fd);
	}
	if (readSize < 0) {
		cerr << "Error Occurred\n";
		return -1;
	}
	else {

		return len;
	}
}


 //send all file in binary
int send_all_binary(int serverfd, FILE *fd, int size) {
	char buf[100000];
	int sentSize = 0;
	while (size > 0) {
		int readSize = fread(buf, 1, 99999, fd);
		int stat = send_all(serverfd, buf, readSize);
		if (stat != 0) {
			cout << "error in sending" << endl;
			return -1;
		}
		size = size - readSize;
	}
	return 0;
}


// read a line until ""\r\n"
int readWholeLine(int serverfd, string& result) {
	char buf[1000];
	int readSize = 0;
	result = remBuf;
	do {
		result += string(buf, buf + readSize);
		int pos = result.find("\r\n");
		if (pos != string::npos) {
			//found
			remBuf = result.substr(pos + 2);
			result = result.substr(0, pos + 2);
			break;
		}
	} while ((readSize = recv(serverfd, buf, 99, 0)) > 0);

	return 0;
}

int reverse_port_string(string &in, string &ipstr, string &portstr) {
	int cnt = 0, pos;;
	string ip = in;
	for (int i = 0; i < in.size(); ++i)
	{
		if (ip[i] == ',') {
			ip[i] = '.';
			cnt++;
			if (cnt == 4) {
				pos = i;
				break;
			}
		}
	}

	if (cnt != 4) return -1;
	ipstr = ip.substr(0, pos);
	string port = ip.substr(pos + 1);
	int val = 0;
	int i = 0;

	while (i < port.size()) {
		if (port[i] == ',') break;
		val = 10 * val + (port[i] - '0');
		i++;
	}
	val = 256 * val;
	int portval = val;
	val = 0;
	i++;
	while (i < port.size()) {
		val = 10 * val + (port[i] - '0');
		i++;
	}

	portval = portval + val;
	stringstream ss;
	ss << portval;
	portstr = ss.str();
	return 0;

}

 // function for execute any system command
string execute_cmd(const char* cmd) {
	FILE* pipe = popen(cmd, "r");
	if (!pipe) return "ERROR";
	char buffer[128];
	std::string result = "";
	while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe) != NULL)
			result += buffer;
	}
	pclose(pipe);
	return result;
}

//ftp
void ftp(int clientControlfd) {
	string progver = "Connect to server successfully..\n Type help for comand instruction...\r\n";
	send_all(clientControlfd, progver.c_str(), progver.size());

	// Handling dummy authentication (This is done to conform to the RFC 959)

	int clientDatafd = 0, datasocket = 0; // clientDatafd is for data connection, datasocket is the socketfd of the data server
	int binarymode = 0; // For binary mode
	while (true) {
		string command;
		readWholeLine(clientControlfd, command);

		 if (command.compare(0, strlen("PORT"), "PORT") == 0) {

			string portstr = command.substr(4); // Getting the ipport string
			portstr = trim(portstr);
			string ip, port;
			reverse_port_string(portstr, ip, port); // Parsing the string.

			datasocket = bindsocket(DATA_PORT); // binds server to supplied data port
			clientDatafd = make_client_connection_with_sockfd(datasocket, ip.c_str(), port.c_str());

			string res = "PORT command successful\r\n";
			send_all(clientControlfd, res.c_str(), res.size());

		}

		//ls cmd
		else if (command.compare(0, strlen("LIST"), "LIST") == 0) {
			if (clientDatafd > 0) {
				string res = "Directory listing...\r\n";
				send_all(clientControlfd, res.c_str(), res.size());
				res = execute_cmd("ls -l");
				send_all(clientDatafd, res.c_str(), res.size());
				close(clientDatafd);
				close(datasocket);
				clientDatafd = 0;
				datasocket = 0;
				res = "Directory listing end...\r\n";
				send_all(clientControlfd, res.c_str(), res.size());
			}
			else {
				string res = "Use PORT\r\n";
				send_all(clientControlfd, res.c_str(), res.size());
			}
		}

		// file transfer
		else if (command.compare(0, strlen("TYPE I"), "TYPE I") == 0) {
			string res = "Switching to Binary mode.\r\n";
			binarymode = 1;
			send_all(clientControlfd, res.c_str(), res.size());
		}

		// get cmd
		else if (command.compare(0, strlen("RETR"), "RETR") == 0) {
			if (clientDatafd <= 0) {
				string res = "Use PORT first\r\n";
				send_all(clientControlfd, res.c_str(), res.size());
				continue;
			}

			string fileName = command.substr(4);
			fileName = trim(fileName);

			// Getting the filesize of the supplied file
			struct stat st;
			int statcode = stat(fileName.c_str(), &st);
			int size = st.st_size;
			if (statcode == -1) {
				close(clientDatafd);
				close(datasocket);
				clientDatafd = 0;
				datasocket = 0;
				string res = string(strerror(errno)) + "\r\n";
				send_all(clientControlfd, res.c_str(), res.size());
				binarymode = 0;
				continue;
			}

			string res = "Changing " + fileName + " to BINARY mode.\r\n";
			send_all(clientControlfd, res.c_str(), res.size());

			FILE * filew;
			int numw, len;
			filew = fopen(fileName.c_str(), "rb");
			len = send_all_binary(clientDatafd, filew, size);
			fclose(filew);
			
			close(clientDatafd);
			close(datasocket);
			clientDatafd = 0;
			datasocket = 0;
			res = "Transfer complete.\r\n";

			send_all(clientControlfd, res.c_str(), res.size());
			binarymode = 0;
			
		}

		// put cmd
		else if (command.compare(0, strlen("STOR"), "STOR") == 0) {
			if (clientDatafd <= 0) {
				string res = "Use PORT first\r\n";
				send_all(clientControlfd, res.c_str(), res.size());
				continue;
			}

			string fileName = command.substr(4);
			fileName = trim(fileName);

			string res = "Ready to send.\r\n";
			send_all(clientControlfd, res.c_str(), res.size());


			FILE * filew;
			int numw;

			filew = fopen(fileName.c_str(), "wb");
			int len = recv_all_binary(clientDatafd, filew);
			fclose(filew);

			close(clientDatafd);
			close(datasocket);
			clientDatafd = 0;
			datasocket = 0;
			res = "Transfer complete.\r\n";
			send_all(clientControlfd, res.c_str(), res.size());
			binarymode = 0;
		}

		// quit cmd
		else if (command.compare(0, strlen("QUIT"), "QUIT") == 0) {

			string res = "Quit successfully...\r\n";
			send_all(clientControlfd, res.c_str(), res.size());
			cout << res;
			close(clientDatafd);
			close(datasocket);
			close(clientControlfd);
			return;

		}

		// wrong cmd
		else {
			string res = "Unknown command.\r\n";
			send_all(clientControlfd, res.c_str(), res.size());
		}
	}

}



int main(int argc, char **argv){
	if(argc == 2){
		CONTROL_PORT = argv[1];
		DATA_PORT= argv[1] + 1;
	}else{
		cout << "USAGE: ./server <SERVER PORT #>" << endl;
		cout << "such as: ./server 2000"<< endl;
		return 0;
	}


	// make server bind and listen to the supplied port
	int server_fd;
	if((server_fd = server_listen(CONTROL_PORT)) <0 ){
		fprintf(stderr, "%s\n", "Error listening to given port");
		return 0;
	}

	cout<<"Server Listening at port# "<<CONTROL_PORT<<endl;
	// now start accept incoming connections:

	while(true){
		int clientControlfd;
		if((clientControlfd = accept_connection(server_fd)) <0){
			fprintf(stderr, "%s\n", "Error Accepting Connections");
		}else{
			int pid = fork();

			if(pid==0){
				//child
				ftp(clientControlfd);
				close(clientControlfd);
				return 0;
			}
		}

	}
	return 0;

}
