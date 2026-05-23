#include "ElectionManager.hh"
#include "Exception.hh"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/un.h>

#include <sstream>
#include <cassert>

#define DEBUG 0

using namespace catlib;

// may throw exception
ElectionManager::ElectionManager(std::string election_key, int election_port): 
  my_key_(election_key), my_port_(election_port), listen_socket_(0), listen_thread_(0),
  ipc_listen_thread_(0),broadcast_socket_(0),respond_socket_(0), ServerPID_(0)
{  
  my_ip_addr_ = get_my_address();
  loopback_addr_ = "127.0.0.1";
  ServerIpAddress_ = "Unknown";

  // multiple election managers may be running on this host
  my_pid_ = getpid();

#if DEBUG > 0
  std::cerr << my_pid_ << "@" << my_ip_addr_ << std::endl;
#endif

  // do all the work!
  make_inquiry();

#if DEBUG > 0
  if(ServerPID_ != 0)
    std::cerr << "Server is " << ServerPID_ << "@" << ServerIpAddress_ << std::endl;
  else
    std::cerr << "Server is " << ServerIpAddress_ << std::endl;
    
#endif

  return;
}

ElectionManager::~ElectionManager()
{
  CloseDown();
}

void ElectionManager::CloseDown()
{
  // shut down sockets
  if(broadcast_socket_ != 0){
    close(broadcast_socket_);
    broadcast_socket_ = 0;
  }
  
  if(respond_socket_ != 0){
    close(respond_socket_);
    respond_socket_ = 0;
  }
  
  if(listen_socket_ != 0){
    close(listen_socket_);
    listen_socket_ = 0;
  }
  
  if(!IPC_Client_Mode_ && !ipc_socket_path_.empty()){
    unlink(ipc_socket_path_.c_str());
    ipc_socket_path_.clear();
  }

#if 0
  if(listen_thread_ != 0){
    pthread_cancel(listen_thread_);
    listen_thread_ = 0;
  }
  
  if(ipc_listen_thread_ != 0){
    pthread_cancel(ipc_listen_thread_);
    ipc_listen_thread_ = 0;
  }
  
  ipc_client_list_sync_.Lock();
  if(ipc_client_list_.size() != 0){
    for(auto client = ipc_client_list_.begin(); client != ipc_client_list_.end(); client++){  
      close(client->socket_id);
    }
  } 
  ipc_client_list_sync_.Unlock();
#endif  
  
}

/////////////////////////////
// ask for server, otherwise do election
void ElectionManager::make_inquiry()
{
  ServerFound_     = false;
  IAmServer_       = false;
  ElectionCalled_  = false;
  IPC_Client_Mode_ = false;
  
  // start the inet listening thread
  Lis_Thread_Lock_.Lock();
  // this sets 'IPC_Client_Mode_ = true' if socket is in use
  open_listening();  
  
  // wait here for listening to begin
  Lis_Thread_Lock_.Lock();
  Lis_Thread_Lock_.Unlock();
  
#if DEBUG > 0
  std::cerr << " IPC_Client_Mode is " << IPC_Client_Mode_ << " for pid " << my_pid_ << std::endl;
#endif
  
  if(!IPC_Client_Mode_){
    // we have an internet socket for listening
    // start the Server ipc listening thread
    // in-case other processes are running on this host
    Lis_Thread_Lock_.Lock();
    open_ipc_listening();
    
    // wait here for ipc listening to begin
    Lis_Thread_Lock_.Lock();
    Lis_Thread_Lock_.Unlock();
  }
  
  if(IPC_Client_Mode_)
    // There is another ElectionManager running on this host
    // ask it for server
    inquire_with_ipc();
  else
    // We have the internet port!
    // ask network for server
    inquire_with_inet();
  
  return;
}

// we are (maybe) in the pool of internet connected candidates
// 
void ElectionManager::inquire_with_inet()
{
  // 1/2 sec timespec
  struct timespec pause;
  pause.tv_sec  = 0;
  pause.tv_nsec = 500000000; // 0.5 seconds
  
  // the message we're sending
  std::string the_message;
  the_message  = "pid: " + std::to_string(my_pid_) + '\n';
  the_message += "key: " + my_key_ + '\n';
  the_message += "cmd: " + ServerInquiry_ + " \n";
  
  // send out messages twice a second for TMOUT_INQUIRY secs 
  // to tell everyone I'm here
  int tries = TMOUT_INQUIRY * 2;
  
  while (tries > 0){
    // broadcast my presence
    if(-1 == broadcast_message(the_message)){
      // exception
      std::ostringstream stringStream;
      stringStream << "ERROR - broadcast_message failed!!" << std::endl;
      Exception_t e;
      e.message = stringStream.str();    
      throw (e);
    }
    
    tries--;
    
    nanosleep(&pause, NULL);
     
    //
    // listening thread may set these
    //
    if(ServerFound_ == true){
      assign_server();
      return;
    }
    
    if(ElectionCalled_ == true){
      elect_server();
      return;
    }
  }
  
  // 4 seconds later...
  // Nobody Answered!
  elect_server();
  
  return;
}

// STEPS for Election:
// sort all network nodes we have heard from
// send nominations to all nodes higher than us on the list
// answer any nominations we receive
// if none of our nominees answer - declare victory
void ElectionManager::elect_server()
{
  // start with all responders
  r_message_lock_.Lock();  
  candidate_list_ = responder_list_;
  r_message_lock_.Unlock();
  
  // my ID
  ProcessID_t myself;
  myself.ip_addr = my_ip_addr_;
  myself.pid     = my_pid_;
  
  // no other candidates
  // use myself!
  if(candidate_list_.empty()){
    
#if DEBUG > 3
    std::cerr << "No other candidates!" << std::endl; 
#endif

    r_message_lock_.Lock();  
    server_list_.push_back(myself);
    r_message_lock_.Unlock();
  
    assign_server();

    return;
  }
  
  //
  // list all Candidates
  //
  
  // make myself a candidate
  candidate_list_.push_back(myself);
  
  // sort list
  //candidate_list_.sort(compare_pids);
  candidate_list_.sort(compare_addrs);
   
#if DEBUG > 3
  debug_print("Candidate list ", candidate_list_, __FILE__, __LINE__);
#endif
  
 //
  // list all Nominees (i.e., candidates >= me)
  //
  
  // generate nominee list
  for(auto itr = candidate_list_.begin(); itr != candidate_list_.end(); itr++){
    if( (itr->ip_addr == my_ip_addr_) && (itr->pid == my_pid_) )
      break;
    else
      nominee_list_.push_back(*itr);
  }
  
#if DEBUG > 3
  debug_print("Nominee list ", nominee_list_, __FILE__, __LINE__);
#endif
  
  // 
  // ask each nominee if they're still in the game
  //
  
  // send nomination message to nominees
  std::string the_message;
  the_message =  "pid: " + std::to_string(my_pid_) + '\n';
  the_message += "key: " + my_key_ + '\n';
  the_message += "cmd: " + Nomination_ + " \n";
  
  for(auto itr = nominee_list_.begin(); itr != nominee_list_.end(); itr++){
    if(-1 == send_message(itr->ip_addr, the_message)){
      // exception
      std::ostringstream stringStream;
      stringStream << "ERROR - send_message failed!!" << std::endl;
      Exception_t e;
      e.message = stringStream.str();    
      throw (e);
    }
  }
  
  //
  // WAIT to hear back!
  //
  sleep(TMOUT_ELECTION);

#if DEBUG > 3
  debug_print("NomAccept'd list ", nomaccept_list_, __FILE__, __LINE__);
#endif
  
  // if no nominee responds
  // claim victory
  r_message_lock_.Lock();
  if(nomaccept_list_.empty())
    server_list_.push_back(myself);
  r_message_lock_.Unlock();
  
  // else, some other nominee
  // has signaled their victory
  assign_server();
  
  return;
}

// finalize the election results
void ElectionManager::assign_server()
{
  r_message_lock_.Lock();
  int number_of_servers = server_list_.size();
  r_message_lock_.Unlock();
  
  ProcessID_t server_id;

  assert((void("Network returned more than one server!"), number_of_servers <= 1));
  
  if(number_of_servers == 1){
    // there is one server on the list!! Perfect.
    ServerFound_ = true;

    r_message_lock_.Lock();
    server_id = server_list_.front();
    r_message_lock_.Unlock();
    
    if((server_id.ip_addr == my_ip_addr_) && (server_id.pid == my_pid_)){
      // I win!
      IAmServer_   = true;
      
      ServerIpAddress_ = server_id.ip_addr;
      ServerPID_       = server_id.pid;
      
      server_response_message_  = "pid: " + std::to_string(my_pid_) + '\n'; 
      server_response_message_ += "key: " + my_key_ + '\n';  
      server_response_message_ += "cmd: " + ServerResponse_ + '\n';  
      
      // tell everyone who we are
      r_message_lock_.Lock();
      for(auto itr = responder_list_.begin(); itr != responder_list_.end(); itr++){
	if(-1 == send_message(itr->ip_addr, server_response_message_)){
	  // exception
	  std::ostringstream stringStream;
	  stringStream << "ERROR - send_message failed!!" << std::endl;
	  Exception_t e;
	  e.message = stringStream.str();    
	  throw (e);
	}
	
	itr->done = true;
      }
      
      r_message_lock_.Unlock();
      
    } else {
      // we have received notification of a server on the network
      // but it isn't this process
      IAmServer_   = false;
      ServerIpAddress_ = server_id.ip_addr;
      ServerPID_       = server_id.pid;
    }
    
#if DEBUG > 4
      std::cerr << "The Server ip-addr is  " << server_id.ip_addr << std::endl;
#endif      

  } else if (number_of_servers == 0) {
    // election is done, but we don't know who won
    // ask the network! 
#if DEBUG > 4
      std::cerr << "Searching! " << __FILE__ << ":" << __LINE__ << std::endl;
#endif
    std::string the_message;
    the_message  = "pid: " + std::to_string(my_pid_) + '\n';
    the_message += "key: " + my_key_ + '\n';
    the_message += "cmd: " + ServerInquiry_ + " \n";
    
    int tries = 2;
    while((!ServerFound_) && (tries > 0)){
      if(-1 == broadcast_message(the_message)){
	// exception
	std::ostringstream stringStream;
	stringStream << "ERROR - broadcast_message failed!!" << std::endl;
	Exception_t e;
	e.message = stringStream.str();    
	throw (e);
      }
      
      if(ServerFound_)
	break;

      sleep(1);
      tries--;
    }
    
    if(ServerFound_) {
      r_message_lock_.Lock();
      server_id = server_list_.front();
      ServerPID_       = server_id.pid;
      ServerIpAddress_ = server_id.ip_addr;
      IAmServer_       = false;
      r_message_lock_.Unlock();
    }
  }
  
  assert((void("Server could not be determined!"), ServerFound_ == true));
  
  // if we have any ipc_clients - let them know about the result
  ipc_client_list_sync_.Lock();

  if(ipc_client_list_.size() != 0){
    char msg_buf[MAX_MSG_LEN];
    memset(msg_buf, 0, MAX_MSG_LEN);
    strncpy(msg_buf, ServerIpAddress_.c_str(), MAX_MSG_LEN);
    
    for(auto client = ipc_client_list_.begin(); client != ipc_client_list_.end(); client++){  
      if(-1 == write_socket(client->socket_id, (char *)msg_buf, MAX_MSG_LEN)){
	// exception
	std::ostringstream stringStream;
	stringStream <<  "IPC socket write_socket() falied!: " << strerror(errno) << std::endl;
	Exception_t e;
	e.message = stringStream.str();    
	throw (e);
      }
    }
  } 
  ipc_client_list_sync_.Unlock();
  
}

//
// list sorting helpers
// low numbers at front of list
//
bool catlib::compare_pids (const catlib::ProcessID_t& first, const catlib::ProcessID_t& second)
{
  return (first.pid < second.pid); // Sorts in ascending order
}
bool catlib::compare_addrs (const catlib::ProcessID_t& first, const catlib::ProcessID_t& second)
{
  return(first.ip_addr < second.ip_addr); // Sorts in ascending order
}

//
// NETWORK CODE (not IPC)
//

//////////
// start thread to collect inet messages we receive
void ElectionManager::open_listening()
{
  pthread_attr_t      thread_attr;

  pthread_attr_init(&thread_attr);

  // this thread will listen to broadcast or direct messages
  int rc = pthread_create(&listen_thread_, &thread_attr, 
			  inet_listen_wrapper, (void *)this);
  
  if(rc != 0){
    // exception
    std::ostringstream stringStream;
    stringStream << "ERROR - cannot make new thread!!" << strerror(errno) << std::endl;
    Exception_t e;
    e.message = stringStream.str();    
    throw (e);
  }
  
  // resources freed when thread terminates
  pthread_detach(listen_thread_);
  
}

// c-wrapper for seperate thread
extern "C" void * inet_listen_wrapper(void * a)
{
  ElectionManager * dis  = (ElectionManager *)a;

  // this blocks...
  dis->listen_to_broadcast();
  
  return(NULL);
}

// listens for inet datagram packages
// and sticks them into a list
void * ElectionManager::listen_to_broadcast()
{    
  struct addrinfo hints, *servinfo;
  memset(&hints, 0, sizeof hints);
  hints.ai_family   = AF_INET;    // set to AF_INET to force IPv4
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags    = AI_PASSIVE; // use my IP
  
  int rv;
  std::string port_string = std::to_string(my_port_);

  if ((rv = getaddrinfo(NULL, port_string.c_str(), &hints, &servinfo)) != 0) {
    // no exception for thread
    std::cerr <<  "ERROR - listener: getaddrinfo " << gai_strerror(rv)  << std::endl;
    exit(EXIT_FAILURE);
  }

  //
  // try to get an internet domain socket
  //
  // loop through all the results and bind to the first we can
  bool SocketInUse = false;
  struct addrinfo *p;
  for(p = servinfo; p != NULL; p = p->ai_next) {

    if((listen_socket_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
      continue;
    
    if(bind(listen_socket_, p->ai_addr, p->ai_addrlen) == -1){
      //perror("listen:bind: ");
      if(errno == EADDRINUSE){
	SocketInUse = true;
      }
      close(listen_socket_);
      listen_socket_ = 0;
      continue;
    }
    
    break;
  }
  
  freeaddrinfo(servinfo);

  //
  // failed to get internet domain socket
  //
  if(SocketInUse){
    // an electionmanager is already running? 
    // another process got the port before us
    // try connecting to that proc as ipc client
    IPC_Client_Mode_ = true;
    Lis_Thread_Lock_.Unlock();
    return NULL;
  }

  // failed, but not because socket is in use
  if (p == NULL) {
    // no exception for thread
    std::cerr <<  "ERROR - listener: failed to bind socket" << std::endl;
    exit(EXIT_FAILURE);
  }
  //
  //
  //

  // signal the calling thread we are starting to listen
  //
  Lis_Thread_Lock_.Unlock();

  // start listening on the inet socket
  //
  int  recvbytes;
  char recvbuf[MAX_MSG_LEN];

  struct sockaddr_storage their_addr;
  socklen_t addr_len = sizeof(their_addr);
  char their_addr_string[INET6_ADDRSTRLEN];

  while(true) {
    
    recvbytes = 0;

    // this will block
    if ((recvbytes = recvfrom(listen_socket_, recvbuf, MAX_MSG_LEN-1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1) {
      // no exception for thread
      std::cerr << "ERROR - inet listener: recvfrom " << strerror(errno) << std::endl;
      exit(EXIT_FAILURE);
    }

    recvbuf[recvbytes] = '\0';

    // put message into struct
    NetMessage_t r_message;
    r_message.dotted_ip = inet_ntop(their_addr.ss_family, &((struct sockaddr_in *)&their_addr)->sin_addr, their_addr_string, sizeof their_addr_string);
    r_message.message = recvbuf;

    // put it on list
    ///
    r_message_lock_.Lock();
    ///

    r_message_list_.push_back(r_message);

    // place message into one of several lists
    parse_message_list();

    // reply to ServerInquiry
    if(IAmServer_){
      if(-1 == server_responds()){
	// no exception for thread
	std::cerr <<  "ERROR - listener: server cannot respond!" << std::endl;
	exit(EXIT_FAILURE);
      }
    }
    
    // reply to Nomination
    if(std::string::npos != r_message.message.find(Nomination_)){
      if(-1 == candidate_responds()){
	// no exception for thread
	std::cerr <<  "ERROR - listener: candidate cannot respond!" << std::endl;
	exit(EXIT_FAILURE);
      }
    }

    r_message_lock_.Unlock();
  }
  
  // should not reach here
  close(listen_socket_);

  return NULL;  
}

// read through the received messages 
// and store for later action
void ElectionManager::parse_message_list()
{
  // look for protocol-compliant messages
  while (!r_message_list_.empty()) {

    NetMessage_t this_message = r_message_list_.front();
    r_message_list_.pop_front();
    
    // throw out messages not matching our key
    if(std::string::npos == this_message.message.find(my_key_)){
      continue;
    }
    
    // throw out messages we sent to ourselves
    std::string my_pid_string = "pid: " + std::to_string(my_pid_) + '\n';
    if(((this_message.dotted_ip == my_ip_addr_) || (this_message.dotted_ip == loopback_addr_))
       && (std::string::npos != this_message.message.find(my_pid_string))){      
      continue;
    }
    
    // is a server online?
    if(std::string::npos != this_message.message.find(ServerResponse_)){
      // yeah!
      put_message_in_list(this_message, server_list_);
      ServerFound_ = true;
      // we can bail out...
      return;
    }
    
    if(std::string::npos != this_message.message.find(ServerInquiry_)){
      // peer announcing themselves
      put_message_in_list(this_message, responder_list_);

    } else if(std::string::npos != this_message.message.find(Nomination_)){
      // we have been nominated
      put_message_in_list(this_message, nomrecv_list_);
      ElectionCalled_ = true;

    } else if(std::string::npos != this_message.message.find(NomAccepted_)){
      // nominees responding
      put_message_in_list(this_message, nomaccept_list_);
    }
  }
}

// utility for parse_message
void ElectionManager::put_message_in_list(NetMessage_t message, std::list<ProcessID_t> &list)
{
  // yikes - a little ugly
  std::size_t location = message.message.find("pid: ");
  int message_pid = std::stoi(message.message.substr(location + 5, 8));

  ProcessID_t new_id;
  new_id.ip_addr = message.dotted_ip;
  new_id.pid    = (pid_t)message_pid;
  new_id.done   = false;

  std::list<ProcessID_t>::iterator find_iter = list.begin();
  
  bool duplicate = false;
  for(find_iter; find_iter != list.end(); find_iter++){

    if((find_iter->ip_addr == new_id.ip_addr) && 
       (find_iter->pid    == new_id.pid)){
      // duplicate!
      // flip flag ??
      // find_iter->done = false;
      duplicate = true;
      break;
    }
  }
  
  if(!duplicate)
    list.push_back(new_id);
}

// If I am server - this responds to inquiries
int ElectionManager::server_responds()
{
  // post election
  // answer inquiries
  for(auto itr = responder_list_.begin(); itr != responder_list_.end(); itr++){    
    if(! itr->done){
      if(-1 == send_message(itr->ip_addr, server_response_message_))
	return(-1);
      itr->done = true;
    }
  }
  return(0);
}

// If I am a nominee - this responds to nominations
int ElectionManager::candidate_responds()
{
  // during election
  // answer nominations
  std::string nom_accepted_message ;
  nom_accepted_message  = "pid: " + std::to_string(my_pid_) + '\n'; 
  nom_accepted_message += "key: " + my_key_ + '\n';  
  nom_accepted_message += "cmd: " + NomAccepted_  + '\n';  
  
  for(auto itr = nomrecv_list_.begin(); itr != nomrecv_list_.end(); itr++){    
    if(! itr->done){
      if(-1 == send_message(itr->ip_addr, nom_accepted_message))
	return(-1);
      itr->done = true;
    }
  }
  return(0);
}


//
// IPC CODE
//

// we are on a host which already has another EM connected to the
// network and is preventing us from using the network broadcast port.
// The connected EM process will inform us of the election results on
// an ipc socket
void ElectionManager::inquire_with_ipc()
{
  // 1/2 sec timespec
  struct timespec pause;
  pause.tv_sec  = 0;
  pause.tv_nsec = 500000000; // 0.5 seconds
  
  // the message we're sending
  std::string the_message;
  the_message  = "pid: " + std::to_string(my_pid_) + '\n';
  the_message += "key: " + my_key_ + '\n';
  the_message += "cmd: " + ServerInquiry_ + " \n";
  
  // AF_UNIX is local unix domain socket
  int fd;
  if(-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))){
    // exception
    std::ostringstream stringStream;
    stringStream <<  "Failed creating IPC socket: " << strerror(errno) << std::endl;
    Exception_t e;
    e.message = stringStream.str();    
    throw (e);
  }
  
  ipc_socket_path_ = IPC_SOCKET_PATH + std::to_string(my_port_);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  // "abstract socket address" - XXX non portable XXX
  *addr.sun_path = '\0';
  strncpy(addr.sun_path+1, ipc_socket_path_.c_str(), sizeof(addr.sun_path)-2);
  
  // connect to local ipc server
  int tries = TMOUT_INQUIRY * 2;
  int connect_rc = 0;
  
  while (tries > 0){
    // connect to local socket
    if(-1 == (connect_rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr)))){
      //perror("connect");
      tries--;
      
      nanosleep(&pause, NULL);
      continue;
    }
    break;
  } // end while
  
  if(connect_rc == -1){
    // exception
    std::ostringstream stringStream;
    stringStream <<  "IPC socket failed to connect to local host!: " << strerror(errno) << std::endl;
    Exception_t e;
    e.message = stringStream.str();    
    throw (e);
  }
  
  char msg_buf[MAX_MSG_LEN];

  // reset loop timeout
  tries = TMOUT_INQUIRY * 2;

  // local proc with the inet socket may need a moment...
  while (tries > 0){
    // ask local IPC proc for the server name
    memset(msg_buf, 0, MAX_MSG_LEN);
    strncpy(msg_buf, the_message.c_str(),MAX_MSG_LEN);
    
    if(-1 == write_socket(fd, msg_buf, MAX_MSG_LEN)){
      // exception
      Exception_t e;
      std::ostringstream stringStream;
      stringStream <<  "IPC socket failed to write to local host!: " << strerror(errno) << std::endl;
      // "Connecton reset by peer" is probably normal shutdown
      if(std::string::npos == stringStream.str().find("Connecton reset by peer")) 
	e.message = stringStream.str();
      
      throw (e);
    }
    
#if DEBUG > 4
    std::cerr << "IPC Client waiting to read..." << std::endl;
#endif
    
    memset(msg_buf, 0, MAX_MSG_LEN);
    
    // blocks
    if(-1 == read_socket(fd, (char *)msg_buf, MAX_MSG_LEN)){
      // exception
      Exception_t e;
      std::ostringstream stringStream;
      stringStream <<  "IPC socket failed to read from local host!: " << strerror(errno) << std::endl;
      // "Connecton reset by peer" is probably normal shutdown
      if(std::string::npos == stringStream.str().find("Connecton reset by peer")) 
	e.message = stringStream.str();
      
      throw (e);
    }
    
    the_message = msg_buf;
    if(std::string::npos != the_message.find(WRONG_KEY)){
      // exception
      std::ostringstream stringStream;
      stringStream << "Another process (i.e, with a different key) on this host is already in use at this port." << std::endl;
      Exception_t e;
      e.message = stringStream.str();    
      throw (e);
    }
    
    if(0 == strcmp(msg_buf, "Unknown"))
      continue;
    else
      break;	  

  } // end while(tries)

#if DEBUG > 5
  std::cerr << "IPC Client received server:" << std::endl << msg_buf << std::endl;
#endif
  
  ProcessID_t new_server;

  if(strstr(msg_buf,"MyAddr"))
    new_server.ip_addr = my_ip_addr_;
  else
    new_server.ip_addr = msg_buf;
  
  new_server.pid     = 0;      // just *not* our PID please!
  new_server.done    = false;
  server_list_.push_back(new_server);  
  
  assign_server();
  
  return;
}

///////////////////////////////
// start_service to listen to local ipc requests
// ipc server
void ElectionManager::open_ipc_listening()
{
  pthread_attr_t      thread_attr;
  
  pthread_attr_init(&thread_attr);
  
  // this thread will listen to broadcast or direct messages
  int rc = pthread_create(&ipc_listen_thread_, &thread_attr, 
			  ipc_listen_wrapper, (void *)this);
  
  if(rc != 0){
    // exception
    std::ostringstream stringStream;
    stringStream << "ERROR - cannot make new thread!!" << strerror(errno) << std::endl;
    Exception_t e;
    e.message = stringStream.str();    
    throw (e);
  }

  // resources freed when thread terminates
  pthread_detach(ipc_listen_thread_);
  
}

// c-wrapper for seperate thread
extern "C" void * ipc_listen_wrapper(void * a)
{
  ElectionManager * dis  = (ElectionManager *)a;

  // this blocks...
  dis->listen_to_ipc();
  
  return(NULL);
}

// listens for ipc unix-domain packets and puts them in list
// we wait here to accept ipc connections
void * ElectionManager::listen_to_ipc()
{    
  // let calling thread know we're here!
  Lis_Thread_Lock_.Unlock();
  
  int fd;
  if(-1 == (fd = socket(AF_UNIX, SOCK_STREAM, 0))){
    perror("socket error");
    exit(EXIT_FAILURE);
  }

  ipc_socket_path_ = IPC_SOCKET_PATH + std::to_string(my_port_);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;

  // "abstract socket address" - XXX non portable XXX
  *addr.sun_path = '\0';
  strncpy(addr.sun_path+1, ipc_socket_path_.c_str(), sizeof(addr.sun_path)-2);

  if(-1 == bind(fd, (struct sockaddr*)&addr, sizeof(addr))){
    perror("bind error");
    std::cerr << "Check file '" << ipc_socket_path_ << "' and remove if needed." << std::endl;
    exit(EXIT_FAILURE);
  }
  
  if(listen(fd, 10) == -1) {
    perror("listen error");
    exit(EXIT_FAILURE);
  }

#if DEBUG > 5
  std::cerr << "Waiting for IPC to accept connect" << std::endl;
#endif
  
  while(1){
    // wait for new connections while socket is open
    int  sd;    
    
    // will block
    sd = accept(fd, NULL, NULL);
    
    if(sd == -1){     
      perror("accept error");
      exit(EXIT_FAILURE);
    } 
    else {
      // accept successful
      pthread_t       thread_id;
      pthread_attr_t  thread_attr;	
      arg_t           arg;
      
      arg.dis = (void *)this;
      arg.sock = sd;
      
      pthread_attr_init(&thread_attr);
      
#if DEBUG > 5
      std::cerr << "Starting IPC client thread" << std::endl;
#endif

      Req_Thread_Lock_.Lock();

      // this thread will answer requests from ipc_clients
      if(-1 == pthread_create(&thread_id, &thread_attr, gather_ipc_requests_wrapper, (void *)&arg)){
	perror("thread_create error");
	exit(EXIT_FAILURE);
      } 
      
      Req_Thread_Lock_.Lock();
      Req_Thread_Lock_.Unlock();

    }  // end else if(sd == -1)
    
  } // end while(1)
  
}
// this is started in a seperate Server process thread
extern "C" void * gather_ipc_requests_wrapper(void * a)
{
  arg_t * arg = (arg_t *)a;

  ElectionManager * dis  = (ElectionManager *)arg->dis;
  int               sock = arg->sock;

  dis->gather_ipc_clients(sock);
  
  return(NULL);
}


// server starts a thread for each ipc client/connection with this
// method 
void * ElectionManager::gather_ipc_clients(int sock)
{
  int  recvbytes;
  char recvbuf[MAX_MSG_LEN];
  
  struct sockaddr_storage their_addr;
  socklen_t addr_len = sizeof(their_addr);
  char their_addr_string[INET6_ADDRSTRLEN];
  
  Req_Thread_Lock_.Unlock();
  
  // this will block
  while(1){
    recvbytes = read(sock, recvbuf, sizeof(recvbuf));
    
    if(recvbytes == -1) {
      // no exception for thread
      std::cerr << "ERROR - gather_ipc_requests:read: " << strerror(errno) << std::endl;
      exit(EXIT_FAILURE);
    } 
    
    recvbuf[recvbytes] = '\0';

    if(recvbytes > 0)
      break;
  }

  // IPC client has asked for the server
  // if server found, answer
  // if not, put client on list and answer later

  ipc_client_list_sync_.Lock();

#if DEBUG > 5
  std::cerr << "IPC server added socket "<< sock << std::endl;
  std::cerr << recvbuf << std::endl;
#endif

  std::string ipc_client_message = recvbuf;
  
  // throw out messages not matching our key
  if(std::string::npos == ipc_client_message.find(my_key_)){
    // this connection isn't for us!!
    ipc_client_list_sync_.Unlock();

    char msg_buf[MAX_MSG_LEN];
    memset(msg_buf, 0, MAX_MSG_LEN);
    strncpy(msg_buf, WRONG_KEY, MAX_MSG_LEN);

    if(-1 == write_socket(sock, (char *)msg_buf, MAX_MSG_LEN)){
      std::cerr << "Error writing to ipc socket" << std::endl;
      exit(EXIT_FAILURE);
    }
    
    return NULL;
  }
  
  if(ServerFound_){
    // we know who the server is!
    ipc_client_list_sync_.Unlock();

    char msg_buf[MAX_MSG_LEN];
    memset(msg_buf, 0, MAX_MSG_LEN);
    strncpy(msg_buf, ServerIpAddress_.c_str(),MAX_MSG_LEN);

    if(-1 == write_socket(sock, (char *)msg_buf, MAX_MSG_LEN)){
      std::cerr << "Error writing to ipc socket" << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  
  else {
    // server not found yet
    // add client to the ipc client list
    ipc_client_info_t  c_info;
    c_info.thread_id = pthread_self();
    c_info.socket_id = sock;
    
    ipc_client_list_.push_back(c_info);
    ipc_client_list_sync_.Unlock();      
  }
  
  return(NULL);
}

//
// utilities
//

///////////////////////////////
// send message to an address 
int ElectionManager::send_message(std::string ip_addr, std::string& message)
{
  // aquire a socket
  if(!respond_socket_){
    // reuse the socket - as member variable
    if(-1 == (respond_socket_ = socket(AF_INET, SOCK_DGRAM, 0))) {
      // error
      std::cerr << "ERROR - send_message: socket " << strerror(errno) << std::endl;
      return(-1);
    }    
  }

  struct sockaddr_in the_addr;
  memset((char *) &the_addr, 0, sizeof(the_addr));

  the_addr.sin_family = AF_INET;
  the_addr.sin_port   = htons(my_port_);

  if (inet_aton((const char *)ip_addr.c_str(), &the_addr.sin_addr) == 0) {
    // error
    std::cerr << "ERROR - send_message: inet_aton " <<  strerror(errno) << std::endl;      
    return(-1);
  }
  
  if(-1 == sendto(respond_socket_, message.c_str(), message.length(), 0, (const sockaddr *)&the_addr, sizeof the_addr)) {
    // error
    std::cerr << "ERROR - send_message: sendto " <<  strerror(errno) << std::endl;      
    return(-1);
  }

  return(0);
}

///////////////////////////////
// broadcast messages
int  ElectionManager::broadcast_message(std::string& message)
{
  struct sockaddr_in server_addr;
  int                broadcast = 1;
  
  if(!broadcast_socket_){
    // reuse the socket and the addr list
    // DataGram Socket
    if(-1 == (broadcast_socket_ = socket(AF_INET, SOCK_DGRAM, 0))) {
      // error
      std::cerr << "ERROR - broadcaster: socket " << strerror(errno) << std::endl;
      return(-1);
    }
    
    // this call allows broadcast packets to be sent:
    if (-1 == setsockopt(broadcast_socket_, SOL_SOCKET, SO_BROADCAST, &broadcast,
			 sizeof broadcast)){
      // error
      std::cerr << "ERROR - broadcaster: setsockopt (SO_BROADCAST): " <<  strerror(errno) << std::endl;
      return(-1);
    }

    // get broadcast addr list - maybe multiple interfaces
    if (-1 == get_broadcast_addr())
      return(-1);
  }

  auto  ba_itr =  broadcast_addr_list_.begin();
  std::ostringstream errorStream;
  bool broadcast_failed = true;
  
  // sendto broadcast interfaces
  for(ba_itr; ba_itr != broadcast_addr_list_.end(); ba_itr++){

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(my_port_);
    server_addr.sin_addr = ((struct sockaddr_in *)&(ba_itr)->ifr_addr)->sin_addr;
    memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);

    if(-1 == sendto(broadcast_socket_, message.c_str(), message.length(), 0, (struct sockaddr *)&server_addr, sizeof server_addr)) {
      char ipstr[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, (void *)&(server_addr.sin_addr), ipstr, sizeof ipstr);     
      
      // error
      errorStream << "ERROR - broadcaster: sendto " <<  strerror(errno) << std::endl;      
      errorStream << "Network interface " << ba_itr->ifr_name << " (" << ipstr << ") doesn't permit broadcasting!" << std::endl;
      continue;
    }
    
    broadcast_failed = false;
  }

  if(broadcast_failed){
    Exception_t e;
    e.message = errorStream.str();    
    throw (e);
  }
  
  return(0);
}

///////////////////////////////
// possible broadcast addresses
int ElectionManager::get_broadcast_addr()
{
  int sock;
  
  if((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    // error
    std::cerr << "ERROR - get_broadcast_addr: socket " <<  strerror(errno) << std::endl;      
    return(-1);
  }
  
  // get network interface configurations (multiple interfaces)
  // unusual ioctl!!
  char if_buffer[1024];
  struct ifconf net_interface;
  
  net_interface.ifc_buf = if_buffer;
  net_interface.ifc_len = sizeof(if_buffer);;
  
  // this fills net_interface struct and sets len
  if(-1 == make_ioctl(sock, SIOCGIFCONF, *(struct ifreq *)&net_interface))
    return(-1);

  int num_interfaces = net_interface.ifc_len/sizeof(ifreq);
  
  // see which interfaces are running, have broadcast, and are AF_NET
  for(int interface = 0; interface < num_interfaces; interface++){
    
    struct ifreq *this_ifc_req;
    
    this_ifc_req = net_interface.ifc_req + interface;

    // ?????????
    // let's skip docker interfaces
    if(NULL != strstr(this_ifc_req->ifr_name, "docker")) continue;

    // omit non-internet families
    if(this_ifc_req->ifr_addr.sa_family != AF_INET)
      continue;   
    
    if(-1 == make_ioctl(sock, SIOCGIFFLAGS,  *this_ifc_req))
      return(-1);

    // skip loopback
    //if(this_ifc_req->ifr_flags & IFF_LOOPBACK)
    //continue;

    if((this_ifc_req->ifr_flags & IFF_UP) && 
       (this_ifc_req->ifr_flags & IFF_BROADCAST) && 
       (this_ifc_req->ifr_flags & IFF_RUNNING)){

      struct ifreq tmp_req;
      
      // broadcast addr
      memset(&tmp_req, 0, sizeof(tmp_req));
      strcpy(tmp_req.ifr_name, this_ifc_req->ifr_name);     
      
      if(-1 == make_ioctl(sock, SIOCGIFBRDADDR, tmp_req))
	return(-1);
      
      broadcast_addr_list_.push_back(tmp_req);
    }
  }

#if DEBUG > 6
  for(auto itr = broadcast_addr_list_.begin(); itr !=  broadcast_addr_list_.end(); itr++){
    std::cerr << "broadcasting to " << inet_ntoa(((struct sockaddr_in *)&(itr)->ifr_addr)->sin_addr) << std::endl;
  }
#endif
  
  return(0);
}

//////////////////////////////////
// make an ioctl call on a socket
int ElectionManager::make_ioctl(int socket, int request, struct ifreq &the_ifreq)
{
  if(ioctl(socket, request, &the_ifreq) == -1){
    // error
    std::cerr << "ERROR - ioctl " <<  strerror(errno) << std::endl;      
    return(-1);
  }
  return(0);
}

/////////////////////////////
// rewrite this?
std::string ElectionManager::get_my_address()
{
  int status; 
  struct addrinfo *p, hints; 
  struct addrinfo *servinfo; 
  char ipstr[INET_ADDRSTRLEN];
  char hostname[128];
   
  gethostname(hostname, 128);

#if 1

  memset(&hints, 0, sizeof hints); 

  hints.ai_family   = AF_UNSPEC;    
  hints.ai_socktype = SOCK_STREAM;  
  hints.ai_flags    = AI_CANONNAME;
  hints.ai_protocol = 0;
  
  // talks to the DNS
  status = getaddrinfo(hostname, NULL, &hints, &servinfo);
  
  if(status != 0) { 
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status)); 
    exit(EXIT_FAILURE); 
  }       
  
  for (p=servinfo; p!=NULL; p=p->ai_next) { 
    struct in_addr  *addr;  
    addr = (struct in_addr  *)get_in_addr((struct sockaddr *)p->ai_addr);
    inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);     
  } 
  
  freeaddrinfo(servinfo); 

#endif

#if 0
  // used to debug DNS
  struct ifaddrs *myaddrs, *ifa;
  void *in_addr;
  char buf[64];
  
  if(getifaddrs(&myaddrs) != 0)
    {
      perror("getifaddrs");
      exit(1);
    }
  
  for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
    {
      if (ifa->ifa_addr == NULL)
	continue;
      if (!(ifa->ifa_flags & IFF_UP))
	continue;
      
      switch (ifa->ifa_addr->sa_family)
        {
	case AF_INET:
	  {
	    struct sockaddr_in *s4 = (struct sockaddr_in *)ifa->ifa_addr;
	    in_addr = &s4->sin_addr;
	    break;
	  }
	  
	case AF_INET6:
	  {
	    struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
	    in_addr = &s6->sin6_addr;
	    break;
	  }
	  
	default:
	  continue;
        }
      
      if (!inet_ntop(ifa->ifa_addr->sa_family, in_addr, buf, sizeof(buf)))
        {
	  printf("%s: inet_ntop failed!\n", ifa->ifa_name);
        }
      else
        {
	  printf("%s: %s\n", ifa->ifa_name, buf);
        }
    }
  
#endif

  std::string ip_address(ipstr);

  return(ip_address);
}

////////////////////////////////
// get sockaddr, IPv4 or IPv6:
void * ElectionManager::get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

////////////////////////////////
// 
void ElectionManager::debug_print(std::string name,  std::list<ProcessID_t>list, const char * file, int line_no)
{
  // debug
  std::list<ProcessID_t>::iterator itr;
  
  std::cerr << name << list.size() << " members. " << file << ":" << line_no << std::endl;
  
  for(itr = list.begin(); itr != list.end(); itr++){
    std::cerr << (*itr).ip_addr << ":" <<  (*itr).pid << std::endl;
  }
  std::cerr << std::endl;
}

//
// duplicated from DistrubutedRunManager - maybe I need a socket
// library?
//

/////////////////////////////
// basic socket read/write 
int ElectionManager::read_socket(int sd, char * buf, int n)
{
  int count;
  int red;
  
  count = red = 0;
  
  while(count < n){
    
    errno = 0;
    red =  recv(sd, buf, n - count, 0);
    
    if(red > 0){
      count += red;
      buf += red;
      continue;
    }
    else if( red == 0){
      //std::cerr <<  "recv failed: peer has shutdown: " << __FILE__ << ":" << __LINE__ << std::endl;
      return(-2);
    }
    else {
      std::cerr <<  "recv failed: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
      return(-1);
    }

  } /* end while */
  
  return(count);
}

/////////////////////////////
// basic socket read/write 
int ElectionManager::write_socket(int sd, char * buf, int n)
{	
  int count;
  int rit;
  
  count = rit = 0;
  
  while(count < n){

    errno = 0;
    rit = send(sd, buf, n - count, 0);
    
    if(rit > 0){
      count += rit;
      buf += rit;
      continue;
    }
    
    else if(rit == 0){
      //std::cerr <<  "send failed: peer has shutdown: " << __FILE__ << ":" << __LINE__ << std::endl;
      return(-2);
    }
    else {
      std::cerr <<  "send failed: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
      return(-1);
    }

  } /* end while */
  
  return(count);
  
}
