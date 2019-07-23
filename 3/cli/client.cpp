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
#include <unistd.h> // close()
#include <errno.h>
#include <sys/wait.h>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <sys/stat.h>   /* For stat() */
#include <sstream>
#include <dirent.h>
using namespace std;

string remBuf;	//used in function readWholeLine() as a global variable



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

/**
 * server_listen - bind to the supplied port and listen
 * @param  char* port - a string
 * @return int the fd if no error otherwise <0 value which indicates error
 */
int server_listen(const char *port) {
	// Create address structs
	struct addrinfo hints, *res;
	int sock_fd;

	/* Set the structure to all zeros */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int addr_status = getaddrinfo(NULL, port, &hints, &res);
	if (addr_status != 0)
	{
		fprintf(stderr, "Cannot get info\n");
		return -1;
	}

	// Loop through results, connect to first one we can
	struct addrinfo *p;
	for (p = res; p != NULL; p = p->ai_next)
	{
		// Create the socket
		sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock_fd < 0)
		{
			perror("server side socket open FAILED");
			continue;
		}

		// Set socket options
		int yes = 1;
		int opt_status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (opt_status == -1)
		{
			perror("server side setsockopt FAILED");
			exit(1);
		}

		// Bind the socket to the port
		int bind_status = bind(sock_fd, res->ai_addr, res->ai_addrlen);
		if (bind_status != 0)
		{
			close(sock_fd);
			perror("server side socket bind FAILED");
			continue;
		}

		// Bind the first one we can
		break;
	}

	// No binds happened
	if (p == NULL)
	{
		fprintf(stderr, "server side bind FAILED\n");
		return -1;
	}

	// Don't need the structure with address info any more
	freeaddrinfo(res);

	//listen
	if (listen(sock_fd, 100) < 0) {
		perror("listen");
		exit(1);
	}

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
	struct sockaddr_storage connector_addr; // connector's address information
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

	// Setting Timeout
	struct timeval tv;
	tv.tv_sec = 120;  /* 120 Secs Timeout */
	tv.tv_usec = 0;  // Not init'ing this can cause strange errors
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


	int addr_status = getaddrinfo(host, port, &hints, &res);
	if (addr_status != 0)
	{
		fprintf(stderr, "Cannot get address info\n");
		return -1;
	}

	// Loop through results, connect to first one we can
	struct addrinfo *p;
	for (p = res; p != NULL; p = p->ai_next)
	{
		// Create the socket
		sock_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sock_fd < 0)
		{
			perror("client side open socket FAILED");
			continue;
		}

		// Make connection
		int connect_status = connect(sock_fd, p->ai_addr, p->ai_addrlen);
		if (connect_status < 0)
		{
			close(sock_fd);
			perror("client: connect");
			continue;
		}

		// Bind the first one we can
		break;
	}

	// No binds happened
	if (p == NULL)
	{
		fprintf(stderr, "client side connection FAILED\n");
		return -2;
	}

	// Print out IP address
	char s[INET6_ADDRSTRLEN];
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);


	// Don't need the structure with address info any more
	freeaddrinfo(res);

	return sock_fd;
}

//function to send all data in buffer
int send_all(int socket, const void *buffer, size_t length) {
	size_t i = 0;
	for (i = 0; i < length;) {
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
	while (1) {
		char buf[10001];
		int readSize;
		if ((readSize = recv(serverfd, buf, 10000, 0)) > 0) {
			result += string(buf, buf + readSize);
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
		cerr << "Error Occurred";
		return -1;
	}
	else {

		return len;
	}
}


 //send all file in binary
int send_all_binary(int serverfd, FILE *fd, int size) {
	char buf[1000000];
	int sentSize = 0;
	while (size > 0) {
		int readSize = fread(buf, 1, 999999, fd);
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
	char buf[1001];
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
	} while ((readSize = recv(serverfd, buf, 1000, 0)) > 0);

	if (readSize < 0) {
		cerr << "Error Occurred";
		return -1;
	}
	else {
		return 0;
	}

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

 //convert function
string int2str(int k) {
	stringstream ss;
	ss << k;
	return ss.str();
}

 //get ip by givn socket descriptor
string get_ip(int m_sd) {
	struct sockaddr_in localAddress;
	socklen_t addressLength = sizeof(localAddress);
	getsockname(m_sd, (struct sockaddr*)&localAddress, &addressLength);
	return string(inet_ntoa(localAddress.sin_addr));
}



 //Selects a random port and gives its port string by assigning portstr and also giving port in port
void getportstring(string supplied_ip, string& portstr, string& port) {
	for (int i = 0; i < supplied_ip.size(); ++i){
		if (supplied_ip[i] == '.') supplied_ip[i] = ',';
	}
	int portnum = 40001 + rand() % 10;
	string p1 = int2str(portnum / 256);
	string p2 = int2str(portnum % 256);
	portstr = "PORT " + supplied_ip + "," + p1 + "," + p2 + "\r\n";
	port = int2str(portnum);
}


int main(int argc, char **argv){
	remBuf="";
	if(argc<3){
		/* Report an error */
		cout << "USAGE: ./client <HOST IP> <SERVER PORT #>" << endl;
		cout << "such as: ./client 127.0.0.1 1000" << endl;
		return 0;
	}

	int serverfd;
	if( (serverfd = make_client_connection(argv[1],argv[2]) ) > 0 ){
		// Sending out Authentication Information.
		string res;
		readWholeLine(serverfd,res);
		cout<<"Response: "<<res<<endl;

		while(true){
			cout<<"ftp>";
			string userInput;
			getline(std::cin,userInput);
			ltrim(userInput);
			// put cmd
			if(userInput.compare(0,strlen("put"),"put") == 0){
				int pid = fork();
				if(pid != 0){
					int stat;
					wait(&stat);
					readWholeLine(serverfd,res);
					cout<<"Response: "<<res<<endl;

				}
				else{
					string fileName = userInput.substr(3); // Gets the filename from put command
					fileName = trim(fileName);
					// Gets the file size
					struct stat st;
					int statcode = stat(fileName.c_str(), &st);
					int size = st.st_size;
					if(statcode == -1){
						cout<<strerror(errno)<<endl;
						continue;
					}
					// Switching to Binary Mode
					string typei = "TYPE I\r\n";
					send_all(serverfd,typei.c_str(),typei.size());
					readWholeLine(serverfd,res);


					// Getting a random port and corresponding PORT command
					string portstr,port;
					getportstring(get_ip(serverfd),portstr,port);

					// Listening to data server
					int dataportserverfd = server_listen(port.c_str());
					send_all(serverfd,portstr.c_str(),portstr.size());
					readWholeLine(serverfd,res);


					// Sending out the STOR command
					string storstr = "STOR "+fileName+"\r\n";
					send_all(serverfd,storstr.c_str(),storstr.size());
					readWholeLine(serverfd,res);

					int dataportclientfd = accept_connection(dataportserverfd);

					// Opening file and sending Data
					FILE * filew;
					int numw;
					filew=fopen(fileName.c_str(),"rb");
					cout<<"---------------Upload-Processing--------------------\n";
					cout<<"DATA TRANSFERING..."<<endl;
					cout<< "Given File Name: " << fileName << endl
						<< "Bytes sended: " << size << endl;
					int len = send_all_binary(dataportclientfd,filew,size);
					cout << "File Uploaded to server..." << endl;
					cout<<"--------------End-Upload-Processing------------------\n";
					fclose(filew);
					close(dataportclientfd);
					close(dataportserverfd);
					return 0;

				}
			}

			// get cmd
			else if(userInput.compare(0,strlen("get"),"get") == 0){
				int pid = fork();
				if(pid != 0){
					int stat;
					wait(&stat);
					readWholeLine(serverfd,res);
					cout<<"Response: "<<res<<endl;

				}
				else{
					// Switching to Binary Mode

					string typei = "TYPE I\r\n";
					send_all(serverfd,typei.c_str(),typei.size());
					readWholeLine(serverfd,res);

					// Getting a random port and corresponding PORT command
					string portstr,port;
					getportstring(get_ip(serverfd),portstr,port);

					// Listening to data server
					int dataportserverfd = server_listen(port.c_str());
					send_all(serverfd,portstr.c_str(),portstr.size());
					readWholeLine(serverfd,res);

					// Gets the filename from get command
					string fileName = userInput.substr(3);
					fileName = trim(fileName);


					string getstr = "RETR "+fileName+"\r\n";
					send_all(serverfd,getstr.c_str(),getstr.size());
					readWholeLine(serverfd,res);

					int dataportclientfd = accept_connection(dataportserverfd);

					FILE * filew;
					int numw;

					// Receiving data and storing it in file.
					filew=fopen(fileName.c_str(),"wb");
					cout<<"--------------Download-Processing-------------------\n";
					cout<<"DATA TRANSFERING..."<<endl;
					int len = recv_all_binary(dataportclientfd,filew);
					cout<< "Given File Name: " << fileName << endl
						<< "Bytes Received: " << len << endl;
					cout << "File Downloaded to local..." << endl;
					cout<<"-------------End-Download-Processing-----------------\n";
					fclose(filew);
					close(dataportclientfd);
					close(dataportserverfd);
					return 0;

				}
			}

			// ls cmd
			else if(userInput.compare(0,strlen("ls"),"ls") == 0){
				int pid = fork();
				if(pid != 0){
					int stat;
					wait(&stat);
					readWholeLine(serverfd,res);
					cout<<"Response: "<<res<<endl;


				}
				else{
					//child which will receive data
					string portstr,port;
					getportstring(get_ip(serverfd),portstr,port);
					int dataportserverfd = server_listen(port.c_str());
					send_all(serverfd,portstr.c_str(),portstr.size());
					readWholeLine(serverfd,res);

					send_all(serverfd,"LIST\r\n",strlen("LIST\r\n"));
					readWholeLine(serverfd,res);

					int dataportclientfd = accept_connection(dataportserverfd);
					recv_all(dataportclientfd,res);
					cout<<"---------------Server-Directory-List---------------"<<endl;
					cout<<"Response: "<<res<<endl;
					cout<<"-------------END-Server-Directory-List-------------"<<endl;
					close(dataportclientfd);
					close(dataportserverfd);
					return 0;

				}
			}

			// lls cmd
			else if(userInput.compare(0,strlen("lls"),"lls") == 0){
				string localDirDisplay = execute_cmd("ls -l");
				cout << "---------------Local-Directory-List---------------" << endl;
				cout << "Response: Directory listing...\r" << endl;
				cout << localDirDisplay << endl;
				cout << "-------------END-Local-Directory-List-------------" << endl;
				cout << "Directory listing end...\r" << endl;
			}

			// quit cmd
			else if(userInput.compare(0,strlen("quit"),"quit") == 0){
					send_all(serverfd,"QUIT\r\n",strlen("QUIT\r\n"));
					readWholeLine(serverfd,res);
					close(serverfd);
					exit(0);
			}

			// help cmd
			else if(userInput.compare(0,strlen("help"),"help") == 0){
					cout<<"ls \t\t Lists the current server directory.\n"
					<< "lls \t\t Lists the current local directory.\n"
					<< "put <FILENAME> \t Uploads the given filename to server.\n"
					<< "get <FILENAME> \t Downloads the given filename from server.\n"
					<< "quit \t\t Quit.\n";
			}

			// wrong cmd
			else{
				cout<<"UNKNOWN COMMAND"<<endl;
			}
		}

	}else{
		cerr<<"Cannot connect to server"<<endl;
	}

	return 0;

}
