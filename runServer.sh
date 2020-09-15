#!/bin/bash
g++ chat_server.cpp -o chatserver
g++ chat_client.cpp -o chatclient
echo "Done building and compiling client and server. Now running server.."
./chatserver
