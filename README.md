# Cloud-Storage-System

The scenario of this project is similar to Dropbox:
- A user can upload and save his files on the server.
- The clients of the user are running on different hosts.
- When any of client of the user uploads a file, the server has to transmit it to all other clients.
- When a new client connects to the server, the server should transmit all the files, which have been uploaded by the other clients, to the new client immediately.
- If one of the clients is sleeping, the server should send the file data to other clients in a non-blocking way.
- The uploading data only need to be sent to the clients that belong to the same user.
- If the file uploaded by the client already exits(A file with the same file name already exists on the server), it will be replaced.

## Inputs
   1. `./server <port>`
      Please make sure that you execute the server program in this format.
   2. `./client <ip> <port> <username>`
	  Please make sure that you execute the client program in this format.

   3. `put <filename>`
      This command, which is executed on the client side by the user, is to upload your files to the server side.
      Users can transmit any files they want. But these files, after being received by the server, need to be stored in the same directory created for the user on the server side.
      Each file should be sent to the other clients belonging to the same user.
      The file to be uploaded should reside in the same directory with the client program.

   4. `sleep <seconds>`
      This command is to let the client sleep for the specified period of time.

   5. `exit`
      This command is to disconnect with the server and terminate the program.


## Outputs
   1. Welcome message. (displayed at the client side)
```
Welcome to the dropbox-like server: <username>
```
   2. Uploading progess bar. (displayed at the sending client side)
```
[Upload] <filename> Start!
Progress : [######################]
[Upload] <filename> Finish!
```
   3. Downing progess bar. (displayed at the receiving client side)
```
[Download] <filename> Start!
Progress : [######################]
[Download] <filename> Finish!
```
   4. Sleeping count down. (displayed at the client side)
```
sleep 20
The client starts to sleep.
Sleep 1
.
.
Sleep 19
Sleep 20
Client wakes up.   
```
