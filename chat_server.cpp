/* ###################################### */
/* #    TSAM - Project 2: Chatserver    # */
/* #                                    # */
/* #    Þórir Ármann Valdimarsson       # */
/* #    Smári Freyr Guðmundsson         # */
/* #    Snorri Arinbjarnar              # */
/* #                                    # */
/* ###################################### */

// Standard includes
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// System includes
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/in.h>

// Data structure includes
#include <vector>
#include <map>
#include <string>
#include <string.h>

// Time includes
#include <time.h>

// Stream includes
#include <iostream>
#include <sstream>

/* ### Constants ### */

// Buffer sizes
#define MINBUFFERSIZE 16
#define MEDBUFFERSIZE 64
#define LARGEBUFFERSIZE 256
#define XLARGEBUFFERSIZE 2048
#define XXLARGEBUFFERSIZE 8192

// Sockets and ports
#define SOCKET01 0
#define SOCKET02 1
#define SOCKET03 2

#define PORTAMOUNT 3

#define MAXCLIENTSALLOWED 8

#define PORTLOWERBOUND 30000
#define PORTUPPERBOUND 60000

/* ### Namespace ### */
using namespace std;

/* ### Data structures ### */

// There is one configuration for each listening socket. This struct
// contains the port number and address information which each socket
// is bound to.
struct serverConfiguration
{
    int portNumber;
    int serverSocketDescriptor;
    struct sockaddr_in serverSocketAddress;
};

// This struct is used to maintain information of a knock-in-progress
// for a given IP-address. It is used as a value in a map where the
// keys are IP-addresses trying to connect. timeStarted states when
// the first connection knock was made. portAttempts is a vector of
// port numbers that have been attempted.
struct connectionInProgress
{
    vector<int> portAttempts;
    time_t timeStarted;
};

// Each user which has made a connection is allocated an instance of
// this struct. It can be used to find out what socketFd belongs to
// a userName and vice versa. isReceiving tells the server whether a
// chatUser should be sent messages.
struct chatUser
{
    string userName;
    bool isReceiving;
    int socketFd;
};

/* ### Global variables ### */

// This map uses the IP-address of an incoming connection as a key while
// its value is the struct described above.
map<string, struct connectionInProgress> portKnockingMap;

// Vector that contains the users currently connected to the server.
// Each user contains a unique username and a unique socket file descriptor.
vector<chatUser> currentUsers;

// The server's id. Used to get bonus points.
// Can be viewed by the client.
// The client can regenerate a new one as well.
string Id;

// The three ports this server listens to.
// The main port which will be used if the knocking sequence
// is successful is portB. The other two are only used for
// the knock process.
int portA, portB, portC;

// This file descriptor set contains the socket file descriptors currently
// set on this server. It initially contains the three listening sockets but
// subsequent client socket descriptors will be added to it.
fd_set mainFileDescriptorSet;

/* ### Server/Client communication functions ### */

// Processes input from already connected clients. This is essentially the server's API. API commands are
// interpreted here and corresponding function calls are made.
void checkAPI(string input, int socketFileDescriptor);

// Is used in a few cases. Sends a message to <clientSocketDescriptor> whether an action failed or not.
void sendFeedback(bool success, int clientSocketDescriptor);

// This function generates new server id. The fortune and timestamp are generated automatically but the
// client can pick the groupInitials himself.
void setId(string groupInitials);

// This function gets called when the client requests info the server Id. It simply sends the current Id to him.
void sendIdToClient(int clientSocketDescriptor);

// This function takes the vector of current users, parses the contents into a single white-space separated list
// of usernames and sends to the client.
void sendUserListToClient(int clientSocketDescriptor);

// This function loops through all active users, excluding the sender, and sends them message.
void sendMessageToAllUsers(string message, int clientSocketDescriptor);

// This function finds user with socketFd == clientSocketDescriptor and sends him message.
void sendMessageToUser(string message, int clientSocketDescriptor, string receivingUser);

// This function removes the user from the main file descriptor set and closes his connection.
void disconnectUser(int socketFileDescriptor);

/* ### Server-side private functions ### */

// This function is only called once upon server initialization. It is used to dynamically allocate listening ports
// for the server. It scans the ports on the local machine for closed ports. When three consecutive closed ports
// are found, the global port variables are set and the function terminates. It also configures the addresses as well
// as binding them to the listening sockets. Finally, it makes the sockets non-blocking and sets their listening flags.
void initializeServer(struct serverConfiguration *configurations);

// This function is called when the same IP address has made the third knock. It reads its port attempt vector and
// determines if the sequence is correct.
bool checkPortSequence(vector<int> ports);

// Is used by checkAPI to split input strings from client thus: ABCDEFGH... A, B, CDEFGH...
// Is useful to interpret the API commands, to e.g. separate the actual message from the MSG <USER> command.
void splitString(vector<string> &inputCommands, string input);

// This is used when making sure that we do not create > 1 users with the same user name.
// Also used to make sure that messages are not sent to non-existent users.
bool userExists(string user);

// Server start point.
int main(int argv, char *args[])
{
    // Each socket has one configuration.
    serverConfiguration configurations[3];

    // Open the sockets endpoint for incoming connections and make them non-blocking.
    configurations[SOCKET01].serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    configurations[SOCKET02].serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    configurations[SOCKET03].serverSocketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Make the sockets reusable.
    int q = 1;
    setsockopt(configurations[SOCKET01].serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, &q, sizeof(q));
    setsockopt(configurations[SOCKET02].serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, &q, sizeof(q));
    setsockopt(configurations[SOCKET03].serverSocketDescriptor, SOL_SOCKET, SO_REUSEADDR, &q, sizeof(q));

    // Dynamically allocate three consecutive port numbers.
    initializeServer(configurations);

    // Set the inital server Id with hardcoded group initials.
    setId("THSS");

    // We will use file descriptor sets to maintain our incoming socket connections
    // The set main is our main file descriptor set. We start by zeroing it out.
    FD_ZERO(&mainFileDescriptorSet);

    // Add our listening sockets to the set.
    FD_SET(configurations[SOCKET01].serverSocketDescriptor, &mainFileDescriptorSet);
    FD_SET(configurations[SOCKET02].serverSocketDescriptor, &mainFileDescriptorSet);
    FD_SET(configurations[SOCKET03].serverSocketDescriptor, &mainFileDescriptorSet);

    // This address variable is used to peek at incoming connection requests during
    // the knocking sequence. To access the IP address in order to use it as a key
    // for the port knocking map.
    struct sockaddr_in connectingClientAddress;
    socklen_t connectingClientAddressSize = sizeof connectingClientAddress;

    // Server loop. Loops continuously and processes connection requests.
    while (true) {
        // The select() function alters our main set. Thus we must
        // keep a copy of it for every iteration of the loop.
        fd_set mainFileDescriptorSetBackup = mainFileDescriptorSet;

        // We use select() to handle connections from multiple clients. If a new connection
        // passes the port knocking, it is accepted and the client's socket file descriptor
        // is added to the fd_set. It returns when one or more descriptor in the set is ready
        // to be read.
        if (select(FD_SETSIZE, &mainFileDescriptorSetBackup, NULL, NULL, NULL) < 0) {
            perror("SELECT error");
            exit(1);
        }

        // One ore more socket descriptors are ready to read. Next step is to loop through the
        // fd_set and check which ones they are.
        for (int i = 0; i < FD_SETSIZE; i++) {
            // Check if socket file descriptor i is set in mainFileDescriptorSetBackup.
            if (FD_ISSET(i, &mainFileDescriptorSetBackup)) {
                // If i is one of the listening sockets, that means
                // we have a new connection. One of our listening
                // sockets has received a connection request.
                if (i == configurations[SOCKET01].serverSocketDescriptor || i == configurations[SOCKET02].serverSocketDescriptor || i == configurations[SOCKET03].serverSocketDescriptor) {
                    // Here we determine which port is being knocked.
                    int portNum;
                    for (int j = 0; j < PORTAMOUNT; j++) {
                        if (configurations[j].serverSocketDescriptor == i) {
                            cout << "Knock at: " << configurations[j].portNumber << endl;
                            portNum = configurations[j].portNumber;
                        }
                    }

                    // We accept the connection. If the port sequence has not be finished or done incorrectly
                    // this accepted connection will be closed immediately, see below. We need to accept it
                    // temporarily during the knocking sequence to access the client's IP address in order to
                    // maintain his knock sequence status in the knocking map.
                    // The knocking sequence is portA -> portC -> portB
                    // Once a correct knocking sequence has been made, the last knock must have been on portB
                    // (SOCKET02), in that case the connection will not be closed but kept open on that port.
                    int newClientSocketDescriptor = accept(i, (struct sockaddr *)&connectingClientAddress, &connectingClientAddressSize);
                    string clientIpAddress = (string)(inet_ntoa(connectingClientAddress.sin_addr));

                    // If the client address is trying to connect for the first time,
                    // the starting time is set for this IP address.
                    if (portKnockingMap.count(clientIpAddress) == 0) { time(&portKnockingMap[clientIpAddress].timeStarted); }

                    // The port knocking sequence must be performed within 2 minutes. Here we are
                    // measuring the elapsed time sinced first knock.
                    time_t timeNow;
                    time(&timeNow);
                    double elapsedTimeInSec = difftime(timeNow, portKnockingMap[clientIpAddress].timeStarted);

                    // If time since the first time the client connected is more or equal then 120 sec(2 min), a message
                    // is sent to the client that a timeout has occured.
                    if (elapsedTimeInSec >= 120) {
                        char fail[MINBUFFERSIZE] = "TIMEOUT FAIL";
                        send(newClientSocketDescriptor, fail, sizeof fail, 0);
                        close(newClientSocketDescriptor);
                        portKnockingMap[clientIpAddress].portAttempts.clear();
                        break;
                    }

                    // Insert the client address trying to connect to the map
                    // and push the port number attempt into its vector.
                    portKnockingMap[clientIpAddress].portAttempts.push_back(portNum);

                    // If number of attempts are 3 it is time to check if the knocking sequence
                    // is correct, and send the client a success or failure message of the matter.
                    if (portKnockingMap[clientIpAddress].portAttempts.size() == PORTAMOUNT) {

                        // If the sequence is correct the client file descriptor is added to the set
                        // of file descriptors to read from, and a success message is sent to the client.
                        // Else a fail message is sent.
                        if (checkPortSequence(portKnockingMap[clientIpAddress].portAttempts)) {
                            FD_SET(newClientSocketDescriptor, &mainFileDescriptorSet);
                            cout << "Connected!" << endl;
                            char welcome[MINBUFFERSIZE] = "KNOCK SUCCESS";
                            send(newClientSocketDescriptor, welcome, sizeof welcome, 0);
                        }
                        else {
                            char welcome[MINBUFFERSIZE] = "KNOCK FAIL";
                            send(newClientSocketDescriptor, welcome, sizeof welcome, 0);
                            close(newClientSocketDescriptor);
                        }

                        // The vector for this address is cleared so another attempt
                        // can be made should the client disconnect and reconnect later.
                        portKnockingMap[clientIpAddress].portAttempts.clear();
                    }
                    else {
                        // The knocking sequence is unfinished. Close connection.
                        close(newClientSocketDescriptor);
                    }
                }

                // If i is not one of the listening sockets, it must be
                // an outside connection from client already in the fd_set,
                // containing a message.
                else if (i != configurations[SOCKET01].serverSocketDescriptor || i != configurations[SOCKET02].serverSocketDescriptor || i != configurations[SOCKET03].serverSocketDescriptor) {
                    // Receive message from client i.
                    char receiveBuffer[XXLARGEBUFFERSIZE];
                    int bytesReceived = recv(i, receiveBuffer, sizeof receiveBuffer, 0);

                    // The message will be interpreted and proccessed in checkAPI.
                    checkAPI(receiveBuffer, i);
                }
            }
        }
    }

    // Close connections before termination.
    for (size_t i = 0; i < currentUsers.size(); i++) {
       close(currentUsers[i].socketFd);
    }

    FD_ZERO(&mainFileDescriptorSet);

    return 0;
}

/* ### Server/Client communication functions ### */

// Processes input from already connected clients. This is essentially the server's API. API commands are
// interpreted here and corresponding function calls are made.
void checkAPI(string input, int socketFileDescriptor) {
    vector<string> inputCommands;
    splitString(inputCommands, input);

    if (inputCommands[0] == "ID") { sendIdToClient(socketFileDescriptor); }
    else if (inputCommands[0] == "LEAVE") { disconnectUser(socketFileDescriptor); }
    else if (inputCommands[0] == "WHO") { sendUserListToClient(socketFileDescriptor); }
    else if (inputCommands[0] == "MSG" && userExists(inputCommands[1])) { sendMessageToUser(inputCommands[2], socketFileDescriptor, inputCommands[1]); }
    else if (inputCommands[0] == "MSG" && inputCommands[1] == "ALL") { sendMessageToAllUsers(inputCommands[2], socketFileDescriptor); }
    else if (inputCommands[0] == "CHANGE" && inputCommands[1] == "ID" &&inputCommands[2] != "") { setId(inputCommands[2]); }
    else if (inputCommands[0] == "CONNECT") {
        if (inputCommands[1] != "") {
            if (!userExists(inputCommands[1])) {
                chatUser newUser;
                newUser.userName = inputCommands[1];
                newUser.socketFd = socketFileDescriptor;
                newUser.isReceiving = false;
                currentUsers.push_back(newUser);
                sendFeedback(true, socketFileDescriptor);
            }
            else { sendFeedback(false, socketFileDescriptor); }
        }
        else { sendFeedback(false, socketFileDescriptor); }
    }
    else if (inputCommands[0] == "RECV") {
        for (size_t i = 0; i < currentUsers.size(); i++) {
            if (currentUsers[i].socketFd == socketFileDescriptor) {
                currentUsers[i].isReceiving = true;
                break;
            }
        }
    }
}

// Is used in a few cases. Sends a message to <clientSocketDescriptor> whether an action failed or not.
void sendFeedback(bool success, int clientSocketDescriptor) {
    if (success) {
        char sendBackBuffer[MINBUFFERSIZE] = "SUCCESS";
        send(clientSocketDescriptor, sendBackBuffer, sizeof sendBackBuffer, 0);
    }
    else {
        char sendBackBuffer[MINBUFFERSIZE] = "FAIL";
        send(clientSocketDescriptor, sendBackBuffer, sizeof sendBackBuffer, 0);
    }
}

// This function generates new server id. The fortune and timestamp are generated automatically but the
// client can pick the groupInitials himself.
void setId(string groupInitials) {
    // Clear before change.
    Id = "";

    // Get our fortune cookie.
    FILE *stream = popen("fortune -s", "r");
    char inStream[XLARGEBUFFERSIZE];
    while (fgets(inStream, XLARGEBUFFERSIZE, stream) != NULL)
    {
        Id.append(inStream);
    }
    pclose(stream);

    // Add the timestamp.
    time_t timeStamp;
    time(&timeStamp);
    stringstream timeStream;
    timeStream << timeStamp;
    Id += ctime(&timeStamp);

    // Add the group's initals.
    Id += groupInitials;
}

// This function gets called when the client requests info the server Id. It simply sends the current Id to him.
void sendIdToClient(int clientSocketDescriptor) {
    char sendIdBuffer[XLARGEBUFFERSIZE];
    strcpy(sendIdBuffer, Id.c_str());
    if (send(clientSocketDescriptor, sendIdBuffer, sizeof sendIdBuffer, 0) < 0) { perror("ID_SEND failure"); }
}

// This function takes the vector of current users, parses the contents into a single white-space separated list
// of usernames and sends to the client.
void sendUserListToClient(int clientSocketDescriptor) {
    string userListStringified = "";
    for (size_t i = 0; i < currentUsers.size(); i++) {

        // Format the user list into a sendable string.
        userListStringified.append(currentUsers[i].userName);
        if (i != (currentUsers.size() - 1)) { userListStringified.append(" "); }
    }

    // Send it.
    char sendUsersBuffer[userListStringified.length() + 1];
    memset(sendUsersBuffer, 0, sizeof sendUsersBuffer);
    strcpy(sendUsersBuffer, userListStringified.c_str());
    if (send(clientSocketDescriptor, sendUsersBuffer, sizeof sendUsersBuffer, 0) < 0) { perror("USERLIST_SEND failure"); }
}

// This function loops through all active users, excluding the sender, and sends them message.
void sendMessageToAllUsers(string message, int clientSocketDescriptor) {
    // Find user sending the message.
    string sendingUser = "";
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].socketFd == clientSocketDescriptor) { sendingUser = currentUsers[i].userName; }
    }

    // Assemble the message.
    message = sendingUser + ": " + message;

    char sendMessageBuffer[message.length() + 1];
    strcpy(sendMessageBuffer, message.c_str());

    // Send loop.
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].isReceiving && currentUsers[i].socketFd != clientSocketDescriptor) {
            if (send(currentUsers[i].socketFd, sendMessageBuffer, sizeof sendMessageBuffer, 0) < 0) { perror("server failure: failed to send message"); }
        }
    }
}

// This function finds user with socketFd == clientSocketDescriptor and sends him message.
void sendMessageToUser(string message, int clientSocketDescriptor, string receivingUser) {
    // Find user sending the message and the fd for the receiving user.
    string sendingUser = "";
    int receivingClientSocketDescriptor;
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].socketFd == clientSocketDescriptor) { sendingUser = currentUsers[i].userName; }
        if (currentUsers[i].userName == receivingUser) { receivingClientSocketDescriptor = currentUsers[i].socketFd; }
    }

    // Assemble the message.
    message = "<PRIVATE> " + sendingUser + ": " + message;

    char sendMessageBuffer[message.length() + 1];
    strcpy(sendMessageBuffer, message.c_str());

    // Send the message.
    if (send(receivingClientSocketDescriptor, sendMessageBuffer, sizeof sendMessageBuffer, 0) < 0) { perror("server failure: failed to send message"); }
}

// This function removes the user from the main file descriptor set and closes his connection.
void disconnectUser(int socketFileDescriptor) {
    // Remove the user from fd_set.
    FD_CLR(socketFileDescriptor, &mainFileDescriptorSet);
    vector<chatUser> newUserList;

    // Update the user list.
    for (size_t i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].socketFd != socketFileDescriptor) { newUserList.push_back(currentUsers[i]); }
    }
    currentUsers = newUserList;

    // Close connection.
    close(socketFileDescriptor);
}

/* ### Server-side private functions ### */

// This function is only called once upon server initialization. It is used to dynamically allocate listening ports
// for the server. It scans the ports on the local machine for closed ports. When three consecutive closed ports
// are found, the global port variables are set and the function terminates. It also configures the addresses as well
// as binding them to the listening sockets. Finally, it makes the sockets non-blocking and sets their listening flags.
void initializeServer(struct serverConfiguration *configurations) {
    // Configure the addresses.
    configurations[SOCKET01].serverSocketAddress.sin_family = AF_INET;
    configurations[SOCKET01].serverSocketAddress.sin_addr.s_addr = INADDR_ANY;

    configurations[SOCKET02].serverSocketAddress.sin_family = AF_INET;
    configurations[SOCKET02].serverSocketAddress.sin_addr.s_addr = INADDR_ANY;

    configurations[SOCKET03].serverSocketAddress.sin_family = AF_INET;
    configurations[SOCKET03].serverSocketAddress.sin_addr.s_addr = INADDR_ANY;

    // Find three available and consecutive ports. We simply portscan
    // the range from PORTLOWERBOUND to PORTUPPERBOUND and check any
    // three ports at a time.
    for (int i = PORTLOWERBOUND; i < PORTUPPERBOUND - 2; i++) {
        portA = i;
        portB = i + 1;
        portC = i + 2;

        configurations[SOCKET01].serverSocketAddress.sin_port = htons(portA);
        configurations[SOCKET02].serverSocketAddress.sin_port = htons(portB);
        configurations[SOCKET03].serverSocketAddress.sin_port = htons(portC);

        if (connect(configurations[SOCKET01].serverSocketDescriptor, (struct sockaddr *)&configurations[SOCKET01].serverSocketAddress, sizeof(configurations[SOCKET01].serverSocketAddress)) < 0) {
            if (connect(configurations[SOCKET02].serverSocketDescriptor, (struct sockaddr *)&configurations[SOCKET02].serverSocketAddress, sizeof(configurations[SOCKET02].serverSocketAddress)) < 0) {
                if (connect(configurations[SOCKET03].serverSocketDescriptor, (struct sockaddr *)&configurations[SOCKET03].serverSocketAddress, sizeof(configurations[SOCKET03].serverSocketAddress)) < 0) {
                    configurations[SOCKET01].portNumber = portA;
                    configurations[SOCKET02].portNumber = portB;
                    configurations[SOCKET03].portNumber = portC;

                    cout << "Three consecutive closed ports found: ";
                    cout << "PortA = " << portA << ", PortB = " << portB << ", PortC = " << portC << endl << endl;
                    break;
                }
            }
        }
    }

    // Make the sockets non-blocking.
    int q = 1;
    ioctl(configurations[SOCKET01].serverSocketDescriptor, FIONBIO, (char *)&q);
    ioctl(configurations[SOCKET02].serverSocketDescriptor, FIONBIO, (char *)&q);
    ioctl(configurations[SOCKET03].serverSocketDescriptor, FIONBIO, (char *)&q);

    // Assign name to sockets by binding the addresses to them.
    bind(configurations[SOCKET01].serverSocketDescriptor, (struct sockaddr *)&configurations[SOCKET01].serverSocketAddress, sizeof configurations[SOCKET01].serverSocketAddress);
    bind(configurations[SOCKET02].serverSocketDescriptor, (struct sockaddr *)&configurations[SOCKET02].serverSocketAddress, sizeof configurations[SOCKET02].serverSocketAddress);
    bind(configurations[SOCKET03].serverSocketDescriptor, (struct sockaddr *)&configurations[SOCKET03].serverSocketAddress, sizeof configurations[SOCKET03].serverSocketAddress);

    // Mark the sockets as open for connections.
    // The knock descriptors only need to listen to one client.
    // The descriptor we will be using will receive up to 8 clients.
    listen(configurations[SOCKET01].serverSocketDescriptor, 1);
    listen(configurations[SOCKET02].serverSocketDescriptor, MAXCLIENTSALLOWED);
    listen(configurations[SOCKET03].serverSocketDescriptor, 1);
}

// This function is called when the same IP address has made the third knock. It reads its port attempt vector and
// determines if the sequence is correct.
bool checkPortSequence(vector<int> ports) {
    // The sequence is portA -> portC -> portB.
    return ports[SOCKET01] == portA && ports[SOCKET02] == portC && ports[SOCKET03] == portB;
}

// Is used by checkAPI to split input strings from client thus: ABCDEFGH... A, B, CDEFGH...
// Is useful to interpret the API commands, to e.g. separate the actual message from the MSG <USER> command.
void splitString(vector<string> &inputCommands, string input) {
    string tmp = "";
    int counter = 0;
    for (int i = 0; i < input.length(); i++) {
        if (input[i] == ' ' && counter < 2) {
            inputCommands.push_back(tmp);
            tmp = "";
            counter++;
        }
        else { tmp += input[i]; }
    }
    inputCommands.push_back(tmp);
}

// This is used when making sure that we do not create > 1 users with the same user name.
// Also used to make sure that messages are not sent to non-existent users.
bool userExists(string user) {
    for (int i = 0; i < currentUsers.size(); i++) {
        if (currentUsers[i].userName == user) { return true; }
    }
    return false;
}
