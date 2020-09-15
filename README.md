# ChatRat
ChatRat is a simple chat software made with C/C++ and Berkeley sockets. It was developed and tested
on Ubuntu 18.04.

## How to run
To compile, execute the script runServer.sh which will compile both files and run chatserver for you afterwards.
Afterwards you can run individual instances of the chat_client program and use them to connect to the server.

To use the script you will first need to do this in your bash shell:
```bash
  chmod +x runServer.sh
  ```
  
After that you can execute the script by typing in:
```bash
  ./runServer.sh
  ```
  
After that you will simply need to run one or more clients. For security reasons, the chatserver is implemented such that the client must perform a specific port-knocking sequence to gain access. The client takes three integers as argument, which denote the port numbers you would like to try to knock at. The command below will run a client which will attempt to knock at the port 30000, 30001 and 30002.
```bash
  ./chatclient 30000 30001 30002
  ```
The client will attempt all possible sequences of the given port numbers. For further explanations of this progress, please refer to the code comments.

## To thread or not to thread
In the client we originally intended to use a single thread to run continuously in the background, constantly receciving data from the server and printing. That way the client could perform other actions, such as sending messages and viewing list of online users, but at the same time receive and print messages. We actually implemented it and it worked like a charm... up to a point. For an unknown reason the program got stuck at the blocking recv function inside the thread function, usually when requesting the server for the list of users or the server id. We spent a good deal of time to try and fix this but we eventually decided to go with the clunky (but functioning!) method of using timed receiving mode, see below.

## Not using a thread
You must choose to be in receive mode. Only then you can receive and print incoming messages.
In this mode you cannot send a message and receive at the same time. That is a problem the thread was supposed to solve and it did but created other problems along the way, as described above. The receive mode is timed, such that you can pick a time for how long you will receive messages. After the timeout you will go back to normal mode and be able to send messages yourself and perform other actions.
