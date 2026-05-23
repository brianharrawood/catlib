#include <ctime>
#include <cstring>
#include <csignal>

// catlib
#include "DistributedRunManager.hh"
#include "InputParams.hh"
#include "Exception.hh"
#include "FileIO.hh"

using namespace catlib;

// static assignment of member variables
// unfortunate side effect of signal() extern C call
DistributedRunManager* DistributedRunManager::drm_instance_(nullptr);
std::mutex DistributedRunManager::drm_mutex_;
  
// elect server
DistributedRunManager::DistributedRunManager(InputParams * input, bool use_restart, std::string key, int port) :
  inputParams_(input), NetKey_(key)

{
  zero_it();

  int election_port = server_port_ + 1;

  // don't let election get interrupted
  // it can last several seconds
  sigset_t the_set;
  sigfillset(&the_set);

  // hold signals - except SIGINT
  pthread_sigmask(SIG_BLOCK, &the_set, NULL);

  if(NetKey_.empty()){
    // exception
    std::ostringstream stringStream;
    stringStream << "No Network Key provided!" << std::endl;
    
    catlib::Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }
    
  // will exit on failure
  election_manager_ = new ElectionManager(NetKey_, election_port);

  // release signals
  pthread_sigmask(SIG_UNBLOCK, &the_set, NULL);

  server_name_ = election_manager_->getServerIP();
  
  // launch in selected mode
  if(election_manager_->isServer())
    run_server();  
  else
    run_client();
  
} 

// run as server
DistributedRunManager::DistributedRunManager(DRM_mode_t mode, InputParams * input, bool use_restart, std::string key, int port) : inputParams_(input), NetKey_(key)
{
  zero_it();

  if(mode != SERVER){
    // coding mistake
  }
  
  if(NetKey_.empty()){
    // exception
    std::ostringstream stringStream;
    stringStream << "No Network Key provided!" << std::endl;
    
    catlib::Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }

  if(use_restart)
    RestartFlag_ = 1;
  else
    RestartFlag_ = 0;
  
  // hold get next ticket until a TicketManager is registered
  NextTck_Lock_.Lock();
  
  // only allow one DRM object per process
  std::lock_guard<std::mutex> lock(drm_mutex_);
  if (drm_instance_ == nullptr)
    drm_instance_ = this;
  
  // set up a signalhandler for ^c and sigterm (15)
  // posix sigaction? - no stl implementation yet
  std::signal(SIGINT, static_SignalHandler);
  std::signal(SIGTERM, static_SignalHandler);

  if(port == 0)
    server_port_ = DEFAULT_PORT_NUMBER;
  else
    server_port_ = port;

  server_name_ = "localhost";

  run_server();  

}

// run as client
DistributedRunManager::DistributedRunManager(DRM_mode_t mode, InputParams * input, bool use_restart, std::string server, std::string key, int port) :
  inputParams_(input), NetKey_(key)
{
  zero_it();

  if(mode != CLIENT){
    // coding mistake
  }
  
  if(NetKey_.empty()){
    // exception
    std::ostringstream stringStream;
    stringStream << "No Network Key provided!" << std::endl;
    
    catlib::Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }

  if(use_restart)
    RestartFlag_ = 1;
  else
    RestartFlag_ = 0;
  
  // hold get next ticket until a TicketManager is registered
  NextTck_Lock_.Lock();
  
  // only allow one DRM object per process
  std::lock_guard<std::mutex> lock(drm_mutex_);
  if (drm_instance_ == nullptr)
    drm_instance_ = this;
  
  // set up a signalhandler for ^c and sigterm (15)
  // posix sigaction? - no stl implementation yet
  std::signal(SIGINT, static_SignalHandler);
  std::signal(SIGTERM, static_SignalHandler);

  if(port == 0)
    server_port_ = DEFAULT_PORT_NUMBER;
  else
    server_port_ = port;

  server_name_ = server;

  run_client();  

}

DistributedRunManager::~DistributedRunManager()
{
  delete election_manager_;
  
  if(RESTART_read_stream_)
    fclose(RESTART_read_stream_);
  
  if(RESTART_write_stream_)
    fclose(RESTART_write_stream_);

  if(drm_instance_ == this)
    drm_instance_ = nullptr;
  
}

// signal interruption
// close network down gracefully when ^c raised  
void DistributedRunManager::SignalHandler(int signum)
{
  // handling code

  // clear election_manager sockets
  election_manager_->CloseDown();
  
  if(mode_ == SERVER){
    
    // shut down the listening thread
    pthread_kill(listen_thread_, signum);
    
    // accept no more connections
    close(listen_socket_);

    // cancel all 'connected' threads
    for(auto client_thread : client_list_){
      pthread_t worker_thread = client_thread.thread_id;      
      pthread_kill(worker_thread, signum);
    }

    // flush files
    if(RESTART_read_stream_)
      fclose(RESTART_read_stream_);
    
    if(RESTART_write_stream_)
      fclose(RESTART_write_stream_);
  }
  else {
    // client
    close(client_socket_);
  }

  exit(EXIT_FAILURE);

}

void DistributedRunManager::run_server()
{
  mode_ = SERVER;

  // server does logging 
  init_logging(inputParams_->getOutputDir());

  make_handshake_string();

  make_process_string();

  // open network port to listen for client connections
  if(-1 == init_dispatch()){
    // exception
    std::ostringstream stringStream;
    stringStream << "ERROR - Could not start dispatch" << std::endl;
    
    catlib::Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }

  // caller must create a TickerManager object
  user_ticket_manager_ = 0;
}

void DistributedRunManager::run_client()
{
  mode_ = CLIENT;

  make_handshake_string();

  make_process_string();
  
  // sets client_socket
  if(-1 == init_dispatch()){
    // exception
    std::ostringstream stringStream;
    stringStream << "Could not connect to server dispatcher on host" << server_name_ << std::endl;
    
    catlib::Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }
  
  // get runtime stuff from the server process
  // initialize inputParams
  if(-1 == client_request_parameters(client_socket_)){
    // exception
    std::ostringstream stringStream;
    stringStream << "Could not get input parameters from the server '" << server_name_ << "'" << std::endl;
    
    catlib::Exception_t e;
    e.message = stringStream.str();
    
    throw (e);    
  }
  
  // makes directories if needed
  init_logging(inputParams_->getOutputDir());
  
  // caller must create a TickerManager object
  user_ticket_manager_ = 0;
}


// primary work loop - called by server and client processes
//
int DistributedRunManager::RunTickets()
{
  if(!user_ticket_manager_){
    std::cerr << "ERROR: You must register a TicketManager to use distribution features" << std::endl;
    return(-1);
  }
  
  std::ofstream    output_stream;
  std::streambuf * output_buffer;
  
  Ticket  * the_ticket = user_ticket_manager_->GetNewTicket();

  int       num_complete = 0;
  
  int rc = GetNextTicket(mode_, the_ticket);
  
  while(rc != -1){
    
    // prevent interrupts until ticket is finished
    //block_signals();
    
    // redirect cout to an output file
    assign_cout(the_ticket, &output_stream, &output_buffer);
    
    // run the calculations for this ticket
    user_ticket_manager_->HandleTicket(the_ticket);
    
    // restore cout
    return_cout(&output_stream, &output_buffer);
    
    // user action
    rc = user_ticket_manager_->EndOfTicketAction(current_output_name_, the_ticket);
    
    // user action error - exit loop
    if(rc == -1)
      break;
    
    // the counter
    num_complete++;
    
    // again
    rc = GetNextTicket(mode_, the_ticket);    

    //  ok - handle interruptions
    //unblock_signals();        
  }  
  
  if(mode_ == CLIENT)
    return(0);
  
  // Server waits for clients to finish...
  close_dispatch();
  
  //
  // Then, server process runs through list once more using restart
  // file to pick up any dropped tickets (e.g. from bad network
  // connections)
  //
  if(num_complete == 0){
    // no point rerunning - there is no restart file
    return(0);
  }

  // this tells dispatcher to reset
  user_ticket_manager_->ResetTickets();

  delete the_ticket;
  
  the_ticket = user_ticket_manager_->GetNewTicket();

  // and open restart file
  RestartFlag_ = 2;
  
  rc = GetNextTicket(mode_, the_ticket);
  
  while(rc != -1){
    
    // prevent interrupts until ticket is finished
    block_signals();
        
    std::string ticket_id;

    the_ticket->GetTicketID(ticket_id); 

    std::cerr << "Re-running Ticket " << ticket_id  <<  std::endl;
    
    // redirect cout to an output file
    assign_cout(the_ticket, &output_stream, &output_buffer);

    // run the simulation for this ticket
    user_ticket_manager_->HandleTicket(the_ticket);
    
    // restore cout
    return_cout(&output_stream, &output_buffer);

    // user action
    rc = user_ticket_manager_->EndOfTicketAction(current_output_name_, the_ticket);

    // user action error - exit loop
    if(rc == -1)
      break;

    // again
    rc = GetNextTicket(mode_, the_ticket);

    //  ok - handle interruptions
    unblock_signals();     
  }  

  delete the_ticket;
  
  return(0);
}

// fetch the next ticket to be run
int DistributedRunManager::GetNextTicket(int the_mode, Ticket * the_ticket, char * client_info)
{
  int rc = -1;

  if(the_mode == SERVER){

    Ticket_Lock_.Lock();
    rc = safely_get_ticket(the_ticket, client_info);
    Ticket_Lock_.Unlock();
  } 

  else if(the_mode == CLIENT){
    rc = client_request_next_ticket(client_socket_, the_ticket);
  }

 return(rc);
}


// 
// private
//
void DistributedRunManager::zero_it()
{
  mode_               = UNSPECIFIED;
  RestartFlag_        = 0;
  Verbose_            = 0;
  client_socket_      = 0;
  listen_thread_      = 0;
  listen_socket_      = 0;
  server_port_        = DEFAULT_PORT_NUMBER;
  RESTART_read_stream_  = nullptr;
  RESTART_write_stream_ = nullptr;  
}

// handshake string between server and clent must match
void DistributedRunManager::make_handshake_string()
 {
   std::string UserID = std::to_string((int)getuid());
   
   HandshakeString_ = "Net Distribution Key: ";
   HandshakeString_ += NetKey_;
   HandshakeString_ += "\n";
   HandshakeString_ += "UserID: ";
   HandshakeString_ += UserID;
 }

void DistributedRunManager::make_process_string()
{
  char   cmd_line[S_BUFFER_LEN];
  char   h_name[S_BUFFER_LEN];
  char   my_time[32];

  // gather info
  std::time_t the_time = std::time(nullptr);  
  std::strncpy(my_time, std::ctime(&the_time), 26);

  gethostname(h_name, S_BUFFER_LEN);

  std::string PID = std::to_string((int)getpid());

  // try to get command line information
  std::string proc_name = "/proc/"+PID+"/cmdline";

  FILE * proc_file = fopen(proc_name.c_str(), "r");
  
  if(proc_file){
    
    int rc = fread(cmd_line, 1, S_BUFFER_LEN, proc_file);
    
    fclose(proc_file);
    
    if(rc > 1){
      ProcessString_  = "Command: ";
      ProcessString_ += cmd_line;
      ProcessString_ += "\n";
    }
  }

  // string it together
  ProcessString_ += "Host: ";
  ProcessString_ += h_name;
  ProcessString_ += "\n";
  ProcessString_ += "ProcessID: ";
  ProcessString_ += PID;
  ProcessString_ += "\n";
  ProcessString_ += my_time;
}

int DistributedRunManager::init_dispatch()
{
  if((mode_ != SERVER) && (mode_ != CLIENT))
    return(-1);
  
  if(mode_ == SERVER){

    if(!RestartFlag_){
      // move old restart file out of the way if it exists
      mv_restart_file();
    }

    // get the listening socket
    listen_socket_ = start_listening();
    
    if( -1 == listen_socket_)
      return(-1);
    
    thread_accept(listen_socket_);
  }
  else {
    // Client
    // establish connection to the server process
    client_socket_ =  client_connect_to_server(server_name_);
    
    // spin client for a few seconds to wait for server to become
    // ready
    int MAX_TRIES = 8;
    int NUMBER_OF_TRIES = 0;
    while((-1 == client_socket_) && (NUMBER_OF_TRIES < MAX_TRIES)){
      sleep(1);
      client_socket_ = client_connect_to_server(server_name_);
      NUMBER_OF_TRIES++;
    }

    if(-1 == client_socket_){
      std::cerr <<  "Check host " << server_name_ << " to be sure server process is running on your port number '" << server_port_ << "'." << std::endl; 
      return(-1);
    }

    // server and client handshake strings should agree
    if(0 != client_request_handshake(client_socket_)){
      std::cerr << "Client failed handshake with server process" << std::endl;
      return(-1);
    }

  }
   
  return(0);
}

// waits to clients to complete
int DistributedRunManager::close_dispatch()
{
  // server code only
  // set static flag so safely_get_next_roi knows
  
  // shut down the listening thread
  pthread_cancel(listen_thread_);

  // accept no more connections
  close(listen_socket_);
  
  // let client threads exit
  wait_for_disconnect();
  
  return(0);
}

// thread safety for getting a ticket in server process
// this method is bracketed with mutex locking.
// The old (finished) ticket is passed in by requesting method
int DistributedRunManager::safely_get_ticket(Ticket * the_ticket, char * client_info)
{  
  // restart file update
  mark_as_finished(the_ticket);
  
  std::string ticket_id;

  the_ticket->GetTicketID(ticket_id); 

  if(Verbose_  && the_ticket->IsValid()){

    if(client_info)
      std::cerr << "finished ticket: " << ticket_id  << " on " << client_info << std::endl;
    else
      std::cerr << "finished ticket: " << ticket_id  << " on " << "Server" << std::endl;
  }
  
  int rc = user_ticket_manager_->IncrementTicket(the_ticket);
  
  while((rc != 0) && (RestartFlag_ != 0)){
    
    int fin =  check_if_finished(the_ticket);
    
    if(1 == fin){
      // already have this one
      
      if(Verbose_ && (RestartFlag_ != 2)){
	the_ticket->GetTicketID(ticket_id); 
	
	std::cerr << "restart: skipping ticket: " <<  ticket_id  << std::endl;
      }
      
      rc = user_ticket_manager_->IncrementTicket(the_ticket);
      
      continue;
    }

    break;
  }
  
  if(rc == 0)
    // all finished
    return(-1);
  
  if(Verbose_){
    
    the_ticket->GetTicketID(ticket_id);   

    if(client_info)
      std::cerr << "starting ticket: " <<  ticket_id << " on " << client_info << std::endl;
    else
      std::cerr << "starting ticket: " <<  ticket_id << " on " << "Server" << std::endl;
  }      
  
  return(0);
}


//
// network code for SERVER 
//
int DistributedRunManager::start_listening()
{
  int sock;

  // get an internet domain socket 
  if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1){
    std::cerr << "Failed to get a socket from the system" << std::endl;   
    return(-1);
  }
  
  // Do I need this ??
  //fcntl(sock, F_SETFL, O_NONBLOCK);

  struct sockaddr_in sock_addr;

  memset(&sock_addr, 0, sizeof(struct sockaddr_in));
  
  sock_addr.sin_port = htons((unsigned short)server_port_);
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_addr.s_addr = INADDR_ANY;

  if (::bind(sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr))) {
    // exception
    std::ostringstream stringStream;
    stringStream <<  "bind failed: " << strerror(errno) << std::endl;
    stringStream <<  "Check if there is already running another process on your port number '" << server_port_ << "'." << std::endl; 
    stringStream <<  "run netstat -a | grep " << server_port_ << std::endl;

    Exception_t e;
    e.message = stringStream.str();    
    throw (e);
  }
  
  if (listen(sock, 10)) {
    int the_error = errno;
    if(the_error == EADDRINUSE){
      std::cerr <<  "Another process is using this port!" << std::endl;
    }
    std::cerr <<  "listen failed: " << strerror(the_error) << " " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }

  return(sock);
}

void DistributedRunManager::thread_accept(int sock)
{
  pthread_attr_t      thread_attr;
  pthread_mutexattr_t mutex_attr;
  pthread_mutex_t     the_mutex;

  static Arg_t        arg;

  pthread_attr_init(&thread_attr);
  pthread_mutexattr_init(&mutex_attr);

  arg.dis = this;
  arg.sock = sock;

  // initialize lock mutexes 
  pthread_mutex_init(&the_mutex, &mutex_attr);  

  // this thread will accept new connections from clients
  int rc = pthread_create(&listen_thread_, &thread_attr, 
			  wait_for_connection_wrapper, (void *)&arg);
  
  if(rc != 0)
    std::cerr << "Error starting thread " << __FILE__ << ":" << __LINE__ << std::endl;

  // resources freed when thread terminates
  pthread_detach(listen_thread_);
  

}

// this is started in the 'listen_thread'
extern "C" void * wait_for_connection_wrapper(void * a)
{
  Arg_t * arg = (Arg_t *)a;

  DistributedRunManager * dis  = arg->dis;
  int                     sock = arg->sock;

  dis->wait_for_connection(sock);
  
  return(nullptr);
}

void * DistributedRunManager::wait_for_connection(int sock)
{
  // wait for new connection forever
  while(1){
    int  sd;
    
    struct sockaddr_in new_addr;
    
    socklen_t i = sizeof(new_addr);
    
    // will block
    sd = accept(sock, (struct sockaddr *)&new_addr, &i);
    
    if(sd == -1){     
      std::cerr <<  "accept failed: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
      // exit thread 
      // Exception?
      return(nullptr);
    } 

    else {
      // add this socket/client to list
      pthread_t       thread_id;
      pthread_attr_t  thread_attr;	
      Arg_t           arg;
      
      arg.dis = this;
      arg.sock = sd;
      
      pthread_attr_init(&thread_attr);
      
      // this lock assures thread starts up
      // before processing continues
      GatherReq_Lock_.Lock();

      // this thread will accept new connections from clients
      int rc = pthread_create(&thread_id, &thread_attr, 
			      gather_requests_wrapper, (void *)&arg);
      
      if(rc != 0){
	// only happens in crisis
	std::cerr <<  "Error creating thread: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
	exit(EXIT_FAILURE);
      }
      
      // the request thread is running
      GatherReq_Lock_.Lock();
      GatherReq_Lock_.Unlock();

      client_info_t c_info;
      c_info.thread_id = thread_id;
      c_info.socket_id = sd;
      
      CL_Lock_.Lock();
      client_list_.push_back(c_info);
      CL_Lock_.Unlock();
      
    }  // end else if(sd == -1)
    
  } // end while(1)
  
}

void DistributedRunManager::wait_for_disconnect()
{
  if(client_list_.empty())
    return;
  
  // wait for all 'connected' threads to return
  CL_Lock_.Lock();
  
  for(auto client_thread : client_list_){
    
    pthread_t worker_thread = client_thread.thread_id;
    
    errno = 0;

    if(0 != pthread_join(worker_thread, nullptr)){
      // throw Exception??
      std::cerr <<  "pthread_join failed: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
    }

  }

  client_list_.clear();

  CL_Lock_.Unlock();

  return;
}

// this is started in a new thread for each client
// managed in client_list_
extern "C" void * gather_requests_wrapper(void * a)
{
  Arg_t * arg = (Arg_t *)a;

  DistributedRunManager * dis  = arg->dis;
  int                    sock = arg->sock;

  dis->gather_requests(sock);
  
  return(nullptr);
}


// server starts a thread for each client/connection with this method
// These are all the commands the server knows
// command names are in quotes e.g., "next_ticket_please"
void * DistributedRunManager::gather_requests(int sock)
{
  char   s_buffer[S_BUFFER_LEN];
  char   p_buffer[P_BUFFER_LEN];
  char * s_buf = s_buffer;
  int    n = S_BUFFER_LEN; 

  // signal we're running
  GatherReq_Lock_.Unlock();
  
  while(1){
    // service new requests from clients
    memset(s_buffer, 0, n);
    
    int red = read_socket(sock, s_buf, n);
    
    if(red != n){
      //std::cerr <<  "Failed to read from socket on SERVER: " << __FILE__ << ":" << __LINE__ << std::endl;
      return(nullptr);
    }

    // provide new ticket
    if(strstr(s_buffer, "next_ticket_please"))
      provide_next_ticket(s_buffer, sock);
    
    // provide handshake
    else if(strstr(s_buffer, "handshake")){
      provide_handshake(s_buffer, sock);
    }
    // provide run parameters
    else if(strstr(s_buffer, "parameters"))
      provide_parameters(p_buffer, sock);

    
    else
      {
	std::cerr << "SERVER process received unexpected buffer: " << __FILE__ << ":" << __LINE__ << std::endl;
	std::cerr << "'" << std::string(s_buffer) << "' @" <<  __FILE__ << ":" << __LINE__ << std::endl;
	return(nullptr);
      }
    
  } // end while(1)

  return(nullptr);
}

void DistributedRunManager::provide_handshake(char * buffer, int sock)
{
  int     n = S_BUFFER_LEN; 
  int     writ;
  
  // SERVER is threaded!!
  // lock => HandshakeString
  HandShake_Lock_.Lock();

  std::string clientHandshake(buffer);
  
  // zero buffer
  memset(buffer, 0, n);
  size_t match = 0;
  bool   good_match = false;
  
  match = clientHandshake.find(HandshakeString_);

  if(match != std::string::npos)
    good_match = true;
  else
    good_match = false;
  
  if(!good_match){
    // return 1 to client - fail
    std::cerr << "SERVER process received wrong handshake string: " << __FILE__ << ":" << __LINE__ << std::endl;
    buffer[0] = 0x1;
  } else
    // redundant - memset above
    buffer[0] = 0x0;
  
  writ = write_socket(sock, buffer, n);
  
  if(writ != n){
    std::cerr <<  "error writing to client: " << __FILE__ << ":" << __LINE__ << std::endl;
    HandShake_Lock_.Unlock();
    return;
  }
  
  HandShake_Lock_.Unlock();
}

// previous finished ticket returned from client
// buffer is S_BUFFER_LEN
void DistributedRunManager::provide_next_ticket(char * buffer, int sock)
{
  // the pieces of a network packet
  char command[CMD_LEN];
  char c_host[HOST_LEN];
  char c_pid[PID_LEN];
  
  NextTck_Lock_.Lock();

  Ticket  * client_ticket = user_ticket_manager_->GetNewTicket();

  // unpack finished ticket buffer to ticket
  UnpackNetworkTicket(buffer, (char *)command, (char *)c_host, (char *)c_pid, client_ticket);

  char client_info[S_BUFFER_LEN];

  sprintf(client_info, "%s:%s", c_host, c_pid);

  // request a new ticket
  int rc = GetNextTicket(SERVER, client_ticket, client_info);

  if(rc != 0)
    // no tickets left - signal client
    client_ticket->InValidate();

  // return new ticket to client
  client_ticket->TicketToBuffer(buffer);
  
  delete client_ticket;
  
  int writ = write_socket(sock, buffer, S_BUFFER_LEN);
  
  if(writ != S_BUFFER_LEN){
    // error - Throw Exception??
    std::cerr <<  "error writing to client: " << __FILE__ << ":" << __LINE__ << std::endl;
    NextTck_Lock_.Unlock();
    return;
  }

  NextTck_Lock_.Unlock();
}

void DistributedRunManager::provide_parameters(char * buffer, int sock)
{
  int  n = P_BUFFER_LEN; 
  int  writ;
  
  // SERVER is threaded!!
  Parameter_Lock_.Lock();

  memset(buffer, 0, n);

  std::stringstream params;

  inputParams_->Serialize(params);

  std::string param_string = params.str();
  
  if(param_string.size() > n){
    // code problem!!
    std::cerr << "buffer overflow!!: " << __FILE__ << ":" << __LINE__ << std::endl;
    exit(EXIT_FAILURE);
  }

  param_string.copy(buffer, n, 0);
  
  writ = write_socket(sock, buffer, n);
  
  if(writ != n){
    // error - Throw Exception??
    std::cerr <<  "error writing to client: " << __FILE__ << ":" << __LINE__ << std::endl;
  }

  Parameter_Lock_.Unlock();

}

//
// network code for CLIENT
//
int DistributedRunManager::client_connect_to_server(std::string host)
{
  int sock_desc;

  struct sockaddr_in pin;
  struct hostent    *hp;
  
  // load host entry 
  if ((hp = gethostbyname((const char *)host.c_str())) == 0){ 
    // error
    std::cerr <<  "error getting host by name: " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }
  
  // fill in socket struct 
  memset((char *)&pin, 0, sizeof(pin));
  pin.sin_family = AF_INET;
  pin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
  pin.sin_port = htons((unsigned short)server_port_);
  
  errno = 0;
  // get an internet domain socket 
  if ((sock_desc = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    std::cerr <<  "Failed to get a socket: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }

  errno = 0;
  // connect to port @ host 
  if (connect(sock_desc, (struct sockaddr *)&pin, sizeof(pin)) == -1){
    //std::cerr <<  "Failed to connect to socket: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
    // error - Throw Exception??
    // error reported in init_dispatch
    // this has multiple tries - print error after they are all exhausted
    return(-1);
  }
  
  return(sock_desc);
}

int DistributedRunManager::client_request_handshake(int sock)
{
  char buffer[S_BUFFER_LEN];
  int  n = S_BUFFER_LEN;
    
  static pid_t pid = getpid();
  static char  host[HOST_LEN];
  int          host_len = HOST_LEN;
  static int   hostname = 0;
  int          rc = 0;

  if(!hostname){
    gethostname(host, host_len);
    host[HOST_LEN-1] ='\0';
    hostname = 1;
  } 

  // send server a request
  memset(buffer, 0, n);

  sprintf(buffer, "host %s pid %d handshake %s", host, pid, HandshakeString_.data());

  int writ = write_socket(sock, buffer, n);
  
  if(writ != n){
    std::cerr <<  "Failed to write to server process: " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }

  // read reply
  memset(buffer, 0, n);
  
  int red = read_socket(sock, buffer, n);
  
  if(red != n){
    std::cerr <<  "Failed to read from server process: " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }
  
  if(buffer[0] != 0)
    // handshake doesn't match
    return(-1);
  
  return(0);
  
}

int DistributedRunManager::client_request_parameters(int sock)
{
  char buffer[S_BUFFER_LEN];
  int  n = S_BUFFER_LEN;
  
  static pid_t pid = getpid();
  static char  host[HOST_LEN];
  int          host_len = HOST_LEN;
  static int   hostname = 0;

  if(!hostname){
    gethostname(host, host_len);
    host[255] ='\0';
    hostname = 1;
  } 

  // send server a request
  memset(buffer, 0, n);

  sprintf(buffer, "host %s pid %d parameters", host, pid);
  
  int writ = write_socket(sock, buffer, n);

  if(writ != n){
    std::cerr <<  "Failed to write to server process: " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }
  
  // read reply
  char p_buffer[P_BUFFER_LEN];
  int  p = P_BUFFER_LEN;

  memset(p_buffer, 0, p);
  
  int red = read_socket(sock, p_buffer, p);
  
  if(red != p){
    std::cerr <<  "Failed to read from server process: " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }

  // read parameters
  std::string param_string(p_buffer);
  
  std::stringstream param_stream;
  param_stream.str(param_string);

  inputParams_->Unserialize(param_stream);
    
  return(0);
}

// client returns the finished ticket and fetches next ticket
int DistributedRunManager::client_request_next_ticket(int sock, Ticket * the_ticket)
{
  static char  command[CMD_LEN] = "next_ticket_please";
  static int   have_hostname = 0;
  static char  host[HOST_LEN];
  static char  pid[PID_LEN];

  if(!have_hostname){
    // get host name and pid
    if(-1 == gethostname(host, HOST_LEN)){
      // error
      std::cerr <<  "Failed to gethostname: " << strerror(errno) << " " << __FILE__ << ":" << __LINE__ << std::endl;
      return(-1);
    }
    
    host[HOST_LEN-1] = '\0';

    pid_t my_pid = getpid();

    sprintf(pid, "%d", my_pid);

    have_hostname  = 1;
  } 

  // pack finished ticket into a buffer to return to server
  char   buffer[S_BUFFER_LEN];

  PackNetworkTicket(buffer, (char *)command, (char *)host, (char *)pid, the_ticket);

  int writ = write_socket(sock, buffer, S_BUFFER_LEN);

  if(writ != S_BUFFER_LEN){
    std::cerr <<  "Failed to write to server process: " << __FILE__ << ":" << __LINE__ << std::endl;
    return(-1);
  }

  // read reply from server
  memset(buffer, 0, S_BUFFER_LEN);

  int red = read_socket(sock, buffer, S_BUFFER_LEN);

  if(red != S_BUFFER_LEN){
    std::cerr <<  "Has server process exited? " << std::endl;
    std::cerr <<  "Failed to read from server process! Exiting..." << std::endl;
    return(-1);
  }
    
  the_ticket->BufferToTicket(buffer);

  if(!the_ticket->IsValid()){
    // tickets are all gone
    // std::cerr << "Completed all tickets!" << std::endl;
    return(-1);
  }

  return(0);
}


//
// basic socket read/write 
//
int DistributedRunManager::read_socket(int sd, char * buf, int n)
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

int DistributedRunManager::write_socket(int sd, char * buf, int n)
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

// block all signals
void DistributedRunManager::block_signals()
{
  sigset_t	siginfo;

  sigfillset(&(siginfo));
  sigprocmask(SIG_BLOCK, &(siginfo), nullptr);
}

// unblock all signals
void DistributedRunManager::unblock_signals()
{
  sigset_t	siginfo;

  sigfillset(&(siginfo));
  sigprocmask(SIG_UNBLOCK, &(siginfo), nullptr);
}

void  DistributedRunManager::mark_as_finished(Ticket * finished_ticket)
{
  // starting tickets
  if(!finished_ticket->IsValid())
    return;
  
  Restart_Lock_.Lock();
  
  handle_restart_file(finished_ticket, WRITE_RESTART);

  Restart_Lock_.Unlock();

  return;
}
 
// check restart file to see if this ticket was done previously
int DistributedRunManager::check_if_finished(Ticket * ticket)
{
  Restart_Lock_.Lock();
  
  int rc = handle_restart_file(ticket, READ_RESTART);

  Restart_Lock_.Unlock();

  if((RESTART_read_stream_ == nullptr) && (RestartFlag_ == 1)){
    // user wants to use a restart file but there isn't one!!!
    RestartFlag_ = 0;
  }

  return(rc);
}

void DistributedRunManager::init_logging(std::string output_dir)
{
  // make output dir
  if(output_dir.empty())
    log_dir_ = "./log";
  else {
    log_dir_ = output_dir;
    log_dir_ += "/log";
  }

  FileIO filer;

  filer.make_dir(log_dir_, 0755);
  
#if 0
  std::string cmd = " mkdir -p " + log_dir_;
  
  if(-1 == system(cmd.c_str())){
    std::cerr << "mkdir failed: '" << cmd << "' " <<__FILE__ << ":" << __LINE__ <<  std::endl;
    exit(1);
  }
#endif
  
}

void DistributedRunManager::assign_cout(Ticket * the_ticket, std::ofstream *filestr, std::streambuf ** backup)
{
  std::string ticket_id;
  the_ticket->GetTicketID(ticket_id); 

  char file_name[S_BUFFER_LEN];

  FD_Lock_.Lock();

  std::streambuf *psbuf;

  if(log_dir_.empty())
    sprintf(file_name, "./log_%s", ticket_id.data());
  else 
    sprintf(file_name, "%s/log_%s", log_dir_.c_str(), ticket_id.data());
  
  
  current_output_name_ = file_name;

  filestr->open(file_name);

  std::cout.flush();
  
  *backup = std::cout.rdbuf();     // back up cout's streambuf
  
  psbuf = filestr->rdbuf();        // get file's streambuf

  std::cout.rdbuf(psbuf);          // assign streambuf to cout
 
  FD_Lock_.Unlock();

  // put an informational header in the file
  std::cout << ProcessString_ << std::endl << HandshakeString_ << std::endl << std::endl;
 
  std::cout << "Ticket: " << ticket_id << std::endl << std::endl;

}

void DistributedRunManager::return_cout(std::ofstream *filestr, std::streambuf ** backup)
{
  FD_Lock_.Lock();

  std::cout.flush();

  std::cout.rdbuf(*backup);        // restore cout's original streambuf
  
  filestr->close();

  FD_Lock_.Unlock();
  
}

// runs inside of Restart_Lock_
int DistributedRunManager::handle_restart_file(Ticket * ticket, int restart_mode)
{  
  std::string ticket_id;
  ticket->GetTicketID(ticket_id); 
  
  std::string finished_ticket_id;

  char path[1024];
  strcpy(path, inputParams_->getOutputDir().c_str());
  strcat(path, "/");
  strcat(path, RESTART_FILE);

  std::string restart_file(path);

  long r_line_len = 128;
  char * restart_line = new char[r_line_len];

  switch(restart_mode){
    
  case READ_RESTART:
    // look through file for tickets
    
    // open file
    if(RESTART_read_stream_ == nullptr){
      FD_Lock_.Lock();
      errno = 0;
      RESTART_read_stream_  = fopen(restart_file.c_str(), "r+");
      if(RESTART_read_stream_ == nullptr){
	std::cerr <<  "Failed to open " << restart_file << " for reading. " << strerror(errno) << " at " <<  __FILE__ << ":" << __LINE__ << std::endl;
	FD_Lock_.Unlock();
	delete[] restart_line;
	return(-1);
      }
      FD_Lock_.Unlock();      
    }
    
    // rewind
    fseek(RESTART_read_stream_, 0, SEEK_SET);
    
    while(-1 != getline(&restart_line,  (size_t *)&r_line_len, RESTART_read_stream_)){ 
      
      // strip trailing '\n'
      restart_line[strlen(restart_line)-1] = '\0';

      finished_ticket_id = (const char *)restart_line;

      if(ticket_id == finished_ticket_id){
	// found ticket
	delete[] restart_line;
	return(1);
      }      
    }
    
    break;
    
  case WRITE_RESTART:
    // append newly finished tickets to file
    
    // open fie
    if(RESTART_write_stream_ == nullptr){      
      FD_Lock_.Lock();
      errno = 0;
      RESTART_write_stream_  = fopen(restart_file.c_str(), "a+");
      if(RESTART_write_stream_ == nullptr){
	std::cerr <<  "Failed to open " << restart_file << " for writing. " << strerror(errno) << " at " <<  __FILE__ << ":" << __LINE__ << std::endl;
	FD_Lock_.Unlock();
	delete[] restart_line;
	return(-1);
      }
      FD_Lock_.Unlock();
    }
    
    // write this ticket info to the end of file
    errno = 0;
    int rc = fprintf(RESTART_write_stream_, "%s\n", ticket_id.data());
    
    if(rc < 0){
      std::cerr << "Failed to update " << restart_file << std::endl;
      std::cerr << strerror(errno) << " error reported at " << __FILE__ << ":" << __LINE__ << std::endl;
    }
    fflush(RESTART_write_stream_);
    
    break;    
  }
  
  delete[] restart_line;

  return(0);

}

// move old restart file to file.xx
void DistributedRunManager::mv_restart_file()
{
  struct stat stat_buffer;
  char   the_file_name[1024];
  int  index = 0;

  char path[1024];
  strcpy(path,  inputParams_->getOutputDir().c_str());
  strcat(path, "/");
  strcat(path, RESTART_FILE);

  std::string restart_file(path);

  sprintf(the_file_name, "%s", restart_file.c_str());
  
  while(-1 != stat(the_file_name, &stat_buffer)){
    
    sprintf(the_file_name, "%s.%02d", restart_file.c_str(), index++);
    
  }

  if(!index)
    // no restart file to begin with
    return;

  // else
  std::string command = "mv -f " + restart_file + " " + the_file_name;

  system(command.c_str());

  return;
}

 void  DistributedRunManager::PackNetworkTicket(char * buffer, char * command, char * client_host, char * pid, Ticket * tx_ticket)
 {
   sprintf(buffer, "%s %s %s ^", command, client_host, pid);

   char tx_buffer[TICKET_BUFFER_LEN];

   tx_ticket->TicketToBuffer((char *)tx_buffer);

   strcat(buffer, tx_buffer);

 }

void  DistributedRunManager::UnpackNetworkTicket(char * buffer, char * command, char * client_host, char *client_pid, Ticket * tx_ticket)
 {
   sscanf(buffer, "%s %s %s" , command, client_host, client_pid);

   char * ptr = strchr(buffer, '^');

   if(ptr == nullptr)
     return;

   tx_ticket->BufferToTicket(ptr+1);

 }
