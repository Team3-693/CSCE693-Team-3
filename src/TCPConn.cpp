#include <stdexcept>
#include <strings.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>
#include "TCPConn.h"
#include "strfuncts.h"
#include "PasswdMgr.h"

// The filename/path of the password file
const char pwdfilename[] = "passwd";

TCPConn::TCPConn(){ // LogMgr &server_log):_server_log(server_log) {
   this->PWMgr = std::make_unique<PasswdMgr>(pwdfilename);

}


TCPConn::~TCPConn() {

}

/**********************************************************************************************
 * accept - simply calls the acceptFD FileDesc method to accept a connection on a server socket.
 *
 *    Params: server - an open/bound server file descriptor with an available connection
 *
 *    Throws: socket_error for recoverable errors, runtime_error for unrecoverable types
 **********************************************************************************************/

bool TCPConn::accept(SocketFD &server) {
   return _connfd.acceptFD(server);
}

/**********************************************************************************************
 * sendText - simply calls the sendText FileDesc method to send a string to this FD
 *
 *    Params:  msg - the string to be sent
 *             size - if we know how much data we should expect to send, this should be populated
 *
 *    Throws: runtime_error for unrecoverable errors
 **********************************************************************************************/

int TCPConn::sendText(const char *msg) {
   return sendText(msg, strlen(msg));
}

int TCPConn::sendText(const char *msg, int size) {
   if (_connfd.writeFD(msg, size) < 0) {
      return -1;  
   }
   return 0;
}

/**********************************************************************************************
 * startAuthentication - Sets the status to request username
 *
 *    Throws: runtime_error for unrecoverable types
 **********************************************************************************************/

void TCPConn::startAuthentication() {

   // Skipping this for now
   //_status = s_username;

   _status = s_sendNumber;

   //_connfd.writeFD("Username: "); 

}

/**********************************************************************************************
 * handleConnection - performs a check of the connection, looking for data on the socket and
 *                    handling it based on the _status, or stage, of the connection
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/

void TCPConn::handleConnection() {

   timespec sleeptime;
   sleeptime.tv_sec = 0;
   sleeptime.tv_nsec = 100000000;

   //std::cout << "_status: " << _status << std::endl;

   try {
      switch (_status) {
         case s_username:
            getUsername();
            break;

         case s_passwd:
            getPasswd();
            break;
   
         case s_changepwd:
         case s_confirmpwd:
            changePassword();
            break;

         case s_menu:
            getMenuChoice();

            break;

         case s_sendNumber:
            sendNumber();
            break;

         case s_waitForReply:
            waitForDivisor();
            break;

         default:
            throw std::runtime_error("Invalid connection status!");
            break;
      }
   } catch (socket_error &e) {
      std::cout << "Socket error, disconnecting.";
      disconnect();
      return;
   }

   nanosleep(&sleeptime, NULL);
}

/**********************************************************************************************
 * getUsername - called from handleConnection when status is s_username--if it finds user data,
 *               it expects a username and compares it against the password database
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/

void TCPConn::getUsername() {
   // Insert your mind-blowing code here
   //std::cout << "In useName()" << std::endl; //testing
   //Check if user has inputed name
   if (!_connfd.hasData())
      return;
   std::string userNameInput;
   if (!getUserInput(userNameInput))
      return;
   //lower(userNameInput);

   //store name in object
   this->_username = userNameInput;   
   std::cout << "Got User Name: " << _username << std::endl;//testing

   if (!PWMgr->checkUser(this->_username.c_str()) )
   {
      sendText("Username not recognized\n");
      disconnect();
      std::cout << "Username not found" << std::endl;
   }
   else{

      std::cout << "Username found" << std::endl;
   }
   //transitions to password state
   _connfd.writeFD("Password: "); 
   this->_status = s_passwd;

}

/**********************************************************************************************
 * getPasswd - called from handleConnection when status is s_passwd--if it finds user data,
 *             it assumes it's a password and hashes it, comparing to the database hash. Users
 *             get two tries before they are disconnected
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/

void TCPConn::getPasswd() {
   // Insert your mind-blowing code here
   std::cout << "In getPasswd()" << std::endl; //testing
   //Check if user has inputed passwd
   if (!_connfd.hasData())
      return;
   std::string userPasswdInput;
   if (!getUserInput(userPasswdInput))
      return;
   //lower(userNameInput);

   //store passwd in object
   //this->_ = userPasswdInput; 
   std::cout << "Got User passwd: " << userPasswdInput << std::endl;//testing

   this->PWMgr->checkPasswd(this->_username.c_str(), userPasswdInput.c_str());
}

/**********************************************************************************************
 * changePassword - called from handleConnection when status is s_changepwd or s_confirmpwd--
 *                  if it finds user data, with status s_changepwd, it saves the user-entered
 *                  password. If s_confirmpwd, it checks to ensure the saved password from
 *                  the s_changepwd phase is equal, then saves the new pwd to the database
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/

void TCPConn::changePassword() {
   // Insert your amazing code here
}


/**********************************************************************************************
 * getUserInput - Gets user data and includes a buffer to look for a carriage return before it is
 *                considered a complete user input. Performs some post-processing on it, removing
 *                the newlines
 *
 *    Params: cmd - the buffer to store commands - contents left alone if no command found
 *
 *    Returns: true if a carriage return was found and cmd was populated, false otherwise.
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/

bool TCPConn::getUserInput(std::string &cmd) {
   std::string readbuf;

   // read the data on the socket
   _connfd.readFD(readbuf);

   // concat the data onto anything we've read before
   _inputbuf += readbuf;

   // If it doesn't have a carriage return, then it's not a command
   int crpos;
   if ((crpos = _inputbuf.find("\n")) == std::string::npos)
      return false;

   cmd = _inputbuf.substr(0, crpos);
   _inputbuf.erase(0, crpos+1);

   // Remove \r if it is there
   clrNewlines(cmd);

   return true;
}

/**********************************************************************************************
 * getMenuChoice - Gets the user's command and interprets it, calling the appropriate function
 *                 if required.
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/

void TCPConn::getMenuChoice() {
   if (!_connfd.hasData())
      return;
   std::string cmd;
   if (!getUserInput(cmd))
      return;
   lower(cmd);      

   // Don't be lazy and use my outputs--make your own!
   std::string msg;
   if (cmd.compare("hello") == 0) {
      _connfd.writeFD("Hello back!\n");
   } else if (cmd.compare("menu") == 0) {
      sendMenu();
   } else if (cmd.compare("exit") == 0) {
      _connfd.writeFD("Disconnecting...goodbye!\n");
      disconnect();
   } else if (cmd.compare("passwd") == 0) {
      _connfd.writeFD("New Password: ");
      _status = s_changepwd;
   } else if (cmd.compare("1") == 0) {
      msg += "You want a prediction about the weather? You're asking the wrong Phil.\n";
      msg += "I'm going to give you a prediction about this winter. It's going to be\n";
      msg += "cold, it's going to be dark and it's going to last you for the rest of\n";
      msg += "your lives!\n";
      _connfd.writeFD(msg);
   } else if (cmd.compare("2") == 0) {
      _connfd.writeFD("42\n");
   } else if (cmd.compare("3") == 0) {
      _connfd.writeFD("That seems like a terrible idea.\n");
   } else if (cmd.compare("4") == 0) {

   } else if (cmd.compare("5") == 0) {
      _connfd.writeFD("I'm singing, I'm in a computer and I'm siiiingiiiing! I'm in a\n");
      _connfd.writeFD("computer and I'm siiiiiiinnnggiiinnggg!\n");
   } else {
      msg = "Unrecognized command: ";
      msg += cmd;
      msg += "\n";
      _connfd.writeFD(msg);
   }

}

/**********************************************************************************************
 * sendMenu - sends the menu to the user via their socket
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/
void TCPConn::sendMenu() {
   std::string menustr;

   // Make this your own!
   menustr += "Available choices: \n";
   menustr += "  1). Provide weather report.\n";
   menustr += "  2). Learn the secret of the universe.\n";
   menustr += "  3). Play global thermonuclear war\n";
   menustr += "  4). Do nothing.\n";
   menustr += "  5). Sing. Sing a song. Make it simple, to last the whole day long.\n\n";
   menustr += "Other commands: \n";
   menustr += "  Hello - self-explanatory\n";
   menustr += "  Passwd - change your password\n";
   menustr += "  Menu - display this menu\n";
   menustr += "  Exit - disconnect.\n\n";

   _connfd.writeFD(menustr);
}


/**********************************************************************************************
 * disconnect - cleans up the socket as required and closes the FD
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/
void TCPConn::disconnect() {
   _connfd.closeFD();
}


/**********************************************************************************************
 * isConnected - performs a simple check on the socket to see if it is still open 
 *
 *    Throws: runtime_error for unrecoverable issues
 **********************************************************************************************/
bool TCPConn::isConnected() {
   return _connfd.isOpen();
}

/**********************************************************************************************
 * getIPAddrStr - gets a string format of the IP address and loads it in buf
 *
 **********************************************************************************************/
void TCPConn::getIPAddrStr(std::string &buf) {
   return _connfd.getIPAddrStr(buf);
}

bool TCPConn::isNewIPAllowed(std::string inputIP){
   std::ifstream whitelistFile("whitelist");
   if(!whitelistFile){
      std::cout << "whitelist file not found" << std::endl;
      return false;
   }
   
   if (whitelistFile.is_open()){
      std::string line;
      while(whitelistFile >> line){
         //std::cout << "IP: " << inputIP << ", line: " << line << std::endl;//
         if (inputIP == line){
            std::cout << "New connection IP: "<< inputIP << " , authorized from whitelist" << std::endl;
            return true;
         }
      }
   }

   std::cout << "Match NOT FOUND!" << std::endl;
   return false;

}

void TCPConn::sendNumber(){
   //unsigned int num = 975851579543363;

   std::string numStr;

   numStr = "NUM 975851579543363";

   _connfd.writeFD(numStr);

   _status = s_waitForReply;
}

void TCPConn::waitForDivisor(){
   this->dbNum++;
   
   if(this->dbNum == 20){
      std::string Str("QuitCalc");
      _connfd.writeFD(Str);
   }

   if(this->dbNum == 30){
      std::string Str("NUM 975851579543363");
      _connfd.writeFD(Str);
   }

   if (this->dbNum < 2){
      std::cout << "Waiting for Divisor" << std::endl;
   }

   if (!_connfd.hasData())
   {
      //std::cout << "No Data" << std::endl;
      return;
   }
   std::string cmd;
   if (!getUserInput(cmd))
      return;
   //lower(cmd);

   std::cout << "returned: " << cmd << std::endl;

   _status = s_menu;

}

