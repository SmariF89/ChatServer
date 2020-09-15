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
#include <netinet/in.h>

// Data structure includes
#include <vector>
#include <string>

// Other includes
#include <bits/stdc++.h>
#include <iostream>

/* ### Constants ### */

// Buffer sizes
#define MINBUFFERSIZE 16
#define MEDBUFFERSIZE 64
#define LARGEBUFFERSIZE 256
#define XLARGEBUFFERSIZE 2048
#define XXLARGEBUFFERSIZE 8192

// Other sizes
#define NUMBEROFPORTSTOKNOCK 3
#define ONESEC 1000000

/* ### Namespace ### */
using namespace std;

/* ### Global variables ### */
int socketDescriptor;
string user;

/* ### Split functions ### */

// Split string by first space.
void customInputStringSplitter(vector<string> &inputCommands, string input);
// Split string by all spaces.
void spaceStringSplitter(vector<string> &stringVec, string stringToSplit);

/* ### Print functions ### */

// Print first promt to user.
void printBeginPromptUserMessage();
// Print promt that the user is logged in.
void printLoggedInMessage();
// Print all commands available for user to enter.
void printCommands();
// Print error message and exit the program.
void printErrorAndQuit(string errorMsg);
// Print the standard interface line.
void printLine();

/* ### Other functions ### */

// Use API call CONNECT to validate the user name.
bool userNameIsValid(string input);
// Use API call ID to get and print the server ID.
void getAndPrintServerId();
// Use API call WHO get and print all users currently connected to the server.
void getAndPrintServerUsers();
// Use API calls MSG ALL or MSG to send a message to a specific person or all persons.
void sendMessage(string message, bool sendPrivate);
// Use API calls RECV to enter a receiving mode.
// Enter receive mode to receive messages from other connected users.
// The user must enter a time in seconds that he wiches to be in receive mode.
// After the receive mode, the user is free to make other commands.
void receiveMode();
// Use API calls CHANGE ID to change ID on server.
void changeServerId(string groupInitials);
// Use API calls LEAVE to leave the server and exit the program in a safe way.
bool leaveServerAndQuit();
// Only connect to the server if the knocking sequence is correct. The function takes in
// the ports inputted as arguments and tries to connect to the server with all possible
// port sequences. If timeout occurs or non of the sequences manages to connect,
// the function returns false. If the sequence is correct and a connection is made,
// the function returns true and the socket descriptor of the server is set.
bool connectToServerByPortKnocking(int portArray[]);
// Run the chatroom by calling functions that use the chat server API.
void runChatRoom(string input);


// Client start point.
int main(int argv, char *args[])
{
    // 3 port numbers must be entered as arguments.
    if (argv != 4) { printErrorAndQuit("Please pass in three port numbers as argument."); }

    // Create int array of ports received as arguments, and try to connect to server using connectToServerByPortKnocking function.
    int portArray[NUMBEROFPORTSTOKNOCK] = {atoi(args[1]), atoi(args[2]), atoi(args[3])};
    if(!connectToServerByPortKnocking(portArray)) { printErrorAndQuit("Failed to connect to chat server."); }

    // Receive username from user. Loop will continue to run until a valid username is entered.
    string input;
    while (true) {
        printBeginPromptUserMessage();
        getline(cin, input);
        if (userNameIsValid(input)) { break; }
        else { cout << "Username is not valid, try again." << endl; }
    }


    // Chat room is runned with an inf loop that takes directs the user to various
    // functions that describe the user interface.
    runChatRoom(input);

    close(socketDescriptor);

    return 0;
}

/* ### Split functions ### */

// Split string by first space.
void customInputStringSplitter(vector<string> &stringVec, string stringToSplit) {
    string tmp = "";
    bool firstSpace = true;
    for (int i = 0; i < stringToSplit.length(); i++) {
        if (stringToSplit[i] == ' ' && firstSpace) {
            stringVec.push_back(tmp);
            tmp = "";
            firstSpace = false;
        }
        else { tmp += stringToSplit[i]; }
    }
    stringVec.push_back(tmp);
}

// Split string by all spaces.
void spaceStringSplitter(vector<string> &stringVec, string stringToSplit) {
    string tmp = "";
    for (int i = 0; i < stringToSplit.length(); i++) {
        if (stringToSplit[i] == ' ') {
            stringVec.push_back(tmp);
            tmp = "";
        }
        else if (i == (stringToSplit.length() - 1)) {
            tmp += stringToSplit[i];
            stringVec.push_back(tmp);
            break;
        }
        else { tmp += stringToSplit[i]; }
    }
}

/* ### Print functions ### */

// Print first promt to user.
void printBeginPromptUserMessage() {
    printLine();
    cout << "                             WELCOME TO CHATRAT" << endl;
    printLine();
    cout << "Please enter a username to enter the chat." << endl;
}

// Print promt that the user is logged in.
void printLoggedInMessage() {
    printLine();
    cout << "                         You are logged into the chat!" << endl;
}

// Print all commands available for user to enter.
void printCommands() {
    printLine();
    cout << "                                List of commands" << endl;
    printLine();
    cout << " Input any of the following commands." << endl << endl;
    cout << " snd <message>                 (send message to all users on the chat)" << endl;
    cout << " sndpr <username> <message>    (send a message to a specific user on the chat)" << endl;
    cout << " recv <time in sec>            (Receive messages from users for a given time)" << endl;
    cout << " lst                           (list all users on the chat)" << endl;
    cout << " sid                           (see the id of server)" << endl;
    cout << " changesid <group initials>    (generate new server id with <group initials>)" << endl;
    cout << " esc                           (Leave and disconnect from chat)" << endl << endl;
    cout << " man/help                      (see list of commands available)" << endl;
    printLine();
}

void printLine() {
    cout << "=================================================================================" << endl;
}

// Print error message and exit the program.
void printErrorAndQuit(string errorMsg) {
    cout << errorMsg << endl;
    cout << "Shutting down..." << endl;
    exit(1);
}

/* ### Other functions ### */

// Use API call CONNECT to validate the user name.
bool userNameIsValid(string input) {
    // Create the correct string to send to server.
    input = "CONNECT " + input;
    char sendBuffer[input.length() + 1];
    char receiveBuffer[MINBUFFERSIZE];

    // Zero out("clean") buffers.
    memset(sendBuffer, 0, sizeof sendBuffer);
    memset(receiveBuffer, 0, sizeof receiveBuffer);

    strcpy(sendBuffer, input.c_str());
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Receive response from server if username is valid.
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    if ((string)receiveBuffer == "SUCCESS") { return true; }
    return false;
}

// Use API call ID to get and print the server ID.
void getAndPrintServerId() {
    // Create the correct string to send to server.
    char sendBuffer[MINBUFFERSIZE] = "ID";
    char receiveBuffer[XLARGEBUFFERSIZE];
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Receive response from server containing the server id and print out.
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    printLine();
    fprintf(stderr, "SERVER ID:\n%s\n", receiveBuffer);
    printLine();

    // Zero out("clean") buffers.
    memset(sendBuffer, 0, sizeof sendBuffer);
    memset(receiveBuffer, 0, sizeof receiveBuffer);
}

// Use API call WHO get and print all users currently connected to the server.
void getAndPrintServerUsers() {
    // Create the correct string to send to server.
    char sendBuffer[MINBUFFERSIZE] = "WHO";
    char receiveBuffer[XXLARGEBUFFERSIZE];
    vector<string> receivedUserNamesVector;
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Receive response from server containing the users currently connected.
    recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, 0);

    // Split string by all spaces.
    spaceStringSplitter(receivedUserNamesVector, receiveBuffer);

    printLine();
    cout << "CONNECTED USERS:" << endl;
    for (size_t i = 0; i < receivedUserNamesVector.size(); i++) { cout << receivedUserNamesVector[i] << endl; }
    printLine();

    // Zero out("clean") buffers.
    memset(sendBuffer, 0, sizeof sendBuffer);
    memset(receiveBuffer, 0, sizeof receiveBuffer);
}

// Use API calls MSG ALL or MSG to send a message to a specific person or all persons.
void sendMessage(string message, bool sendPrivate) {
    if (!sendPrivate) { message = "MSG ALL " + message; }
    if (sendPrivate) { message = "MSG " + message; }

    char sendBuffer[message.length() + 1];

    // Zero out("clean") buffer.
    memset(sendBuffer, 0, sizeof sendBuffer);

    strcpy(sendBuffer, message.c_str());

    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);
}

// Use API calls RECV to enter a receiving mode.
// Enter receive mode to receive messages from other connected users.
// The user must enter a time in seconds that he wiches to be in receive mode.
// After the receive mode, the user is free to make other commands.
void receiveMode(int timeOut) {
    cout << "Entering recieve mode for " << timeOut << " seconds." << endl;
    // Create the correct string to send to server.
    char sendBuffer[MINBUFFERSIZE] = "RECV";
    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);

    // Zero out("clean") buffer.
    memset(sendBuffer, 0, sizeof sendBuffer);

    time_t timeStarted, timeElapsed;
    time(&timeStarted);
    // Loop will run until the timeout is reached.
    while (true) {
        if(difftime(time(&timeElapsed), timeStarted) >= timeOut) {
            cout << "Exiting receive mode.." << endl;
            return;
        }

        char receiveBuffer[XXLARGEBUFFERSIZE];
        int bytesReceieved = recv(socketDescriptor, receiveBuffer, sizeof receiveBuffer, MSG_DONTWAIT);
        // Message is written out if something is received from the server.
        if(bytesReceieved > 0) { cout << receiveBuffer << endl; }
        memset(receiveBuffer, 0, sizeof receiveBuffer);
    }
}

// Use API calls CHANGE ID to change ID on server.
void changeServerId(string groupInitials) {
    // Create the correct string to send to server.
    string changeIdCommand = "CHANGE ID " + groupInitials;
    char sendBuffer[MEDBUFFERSIZE];
    memset(sendBuffer, 0, sizeof sendBuffer);
    strcpy(sendBuffer, changeIdCommand.c_str());

    send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);
}

// Use API calls LEAVE to leave the server and exit the program in a safe way.
bool leaveServerAndQuit() {
    string answer = "";

    while (true) {
        cout << "Are you sure you want to leave? (y/n)" << endl;
        cout << user << ": ";
        cin >> answer;
        if (answer == "y") {
            char sendBuffer[MINBUFFERSIZE] = "LEAVE";
            send(socketDescriptor, sendBuffer, sizeof sendBuffer, 0);
            return true;
        }
        else if (answer == "n"){ return false; }
        else { cout << "Invalid command. Try again." << endl; }
    }
    return false;
}

// Only connect to the server if the knocking sequence is correct. The function takes in
// the ports inputted as arguments and tries to connect to the server with all possible
// port sequences. If timeout occurs or non of the sequences maneges to connect,
// the function returns false. If the sequence is correct and a connection is made,
// the function returns true and the socket descriptor of the server is set.
bool connectToServerByPortKnocking(int portArray[]) {
    cout << "Connecting to chat server..." << endl;
    // Create the address of the socket
    struct sockaddr_in socketAddress;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = INADDR_ANY;

    for (int i = 0; i < NUMBEROFPORTSTOKNOCK; i++) {
        for (int j = 0; j < NUMBEROFPORTSTOKNOCK; j++) {
            for (int k = 0; k < NUMBEROFPORTSTOKNOCK; k++) {
                if (portArray[i] != portArray[j] && portArray[j] != portArray[k] && portArray[i] != portArray[k]) {
                    // Knock 1
                    socketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    socketAddress.sin_port = htons(portArray[i]);
                    connect(socketDescriptor, (struct sockaddr *)&socketAddress, sizeof socketAddress);
                    usleep(ONESEC);
                    close(socketDescriptor);

                    // Knock 2
                    socketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    socketAddress.sin_port = htons(portArray[j]);
                    connect(socketDescriptor, (struct sockaddr *)&socketAddress, sizeof socketAddress);
                    usleep(ONESEC);
                    close(socketDescriptor);

                    // Knock 3
                    socketDescriptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    socketAddress.sin_port = htons(portArray[k]);
                    connect(socketDescriptor, (struct sockaddr *)&socketAddress, sizeof socketAddress);

                    // After three knocks a message is received from the server, stating if the
                    // knocking sequence is a success or not. If it was not a success a sleep is set for 1 sec
                    // and the descriptor is closed. Then another sequence is tried.
                    char receiveBuffer[LARGEBUFFERSIZE];
                    recv(socketDescriptor, &receiveBuffer, sizeof receiveBuffer, 0);

                    if((string)receiveBuffer == "KNOCK SUCCESS") { return true; }
                    else if((string)receiveBuffer == "TIMEOUT FAIL") {
                        close(socketDescriptor);
                        printErrorAndQuit("Timeout: Took to long time to connect.");
                    }
                    else {
                        usleep(ONESEC);
                        close(socketDescriptor);
                    }
                }
            }
        }
    }
    return false;
}

// Run the chatroom by calling functions that use the chat server API.
void runChatRoom(string input) {
    user = input;
    vector<string> inputCommands;
    printLoggedInMessage();
    printCommands();

    // Run loop until user chooses the esc option to leave the chatserver.
    while (true) {
        cout << user << ": ";
        getline(cin, input);
        // Use split function to splitting string on spaces and placing them in order in the vector inputCommands.
        customInputStringSplitter(inputCommands, input);

        // Various function calls depending on input from user.
        if (inputCommands.size() > 2) { cout << "Invalid input, try again(input man/help to se list of commands)." << endl; }
        if (inputCommands[0] == "man" || inputCommands[0] == "help") { printCommands(); }
        else if (inputCommands[0] == "snd") { if (inputCommands[1].size() > 0) { sendMessage(inputCommands[1], false); } }
        else if (inputCommands[0] == "sndpr") { if (inputCommands[1].size() > 0) { sendMessage(inputCommands[1], true); } }
        else if (inputCommands[0] == "lst") { getAndPrintServerUsers(); }
        else if (inputCommands[0] == "sid") { getAndPrintServerId(); }
        else if (inputCommands[0] == "changesid") { changeServerId(inputCommands[1]); }
        else if (inputCommands[0] == "esc") {if (leaveServerAndQuit()) { break;} }
        else if (inputCommands[0] == "recv") { if(atoi(inputCommands[1].c_str()) > 0) { receiveMode(atoi(inputCommands[1].c_str())); } }
        else { cout << "Invalid input, try again(input man/help to se list of commands)." << endl; }

        // After each command, the vector needs to be cleared.
        inputCommands.clear();
    }
}
