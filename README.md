# CPSC-471---Project--FTP-Client&Server--using--Socket-Programming

## Group member:

Lambert Liu   chenxiaoliu_1998@csu.fullerton.edu

Shijie Feng   donfeng97@csu.fullerton.edu

Jacqueline Isabel Cardenas   jacisac@csu.fullerton.edu



## How to run?
1. Compile all file

	$ make

2. Run server first, go to server directory, open terminal in server folder, and do:

	$ ./server <port #>
	
	Example:
	$ ./server 2000

3. Run client second, go to client directory, open terminal in client folder, and do:

	$ ./client <ip address> <port #>
	
	(port # should be same)
	
	Example:
	$ ./client 127.0.0.1 2000


## Command to use:
```
- ls  			Lists the current server directory.

- lls  			Lists the current local directory.

- put <FILENAME>  	Uploads the given filename to server.
	
- get <FILENAME>  	Downloads the given filename from server.

- quit  		Quit.
```
