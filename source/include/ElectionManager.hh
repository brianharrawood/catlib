/**
 * \file ElectionManager.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for the ElectionManager.
 *
 */
#ifndef _ElectionManager_HH_
#define _ElectionManager_HH_

#include <string>
#include <iostream>
#include <vector>
#include <list>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <semaphore.h>

// catlib
#include "SyncLock.hh"

#define IPC_SOCKET_PATH "/tmp/DRM_"
#define TMOUT_INQUIRY   4
#define TMOUT_ELECTION  2
#define MAX_MSG_LEN     512
#define WRONG_KEY       "Wrong Key!!"

namespace catlib {
  
  ///\struct  _ipc_client_info
  ///
  /// For ElectionManager to track local clients 
  ///\code
  /// pthread_t thread_id;
  /// int       socket_id;
  ///\endcode
  ///
  struct _ipc_client_info {
    pthread_t thread_id;
    int       socket_id;
  };
  typedef struct _ipc_client_info ipc_client_info_t;
  
  ///\struct  _extern_c_arg
  ///
  ///  The struct to pass arguments in-and-out of class for
  ///  threading
  ///\code
  ///  DistributedRunManager * dis;
  ///  int                    sock;
  ///\endcode
  ///
  struct _extern_c_arg {
    void  * dis;
    int     sock;
  };
  typedef struct _extern_c_arg arg_t;
  
  extern "C" {
    void * inet_listen_wrapper(void * a);
    void * ipc_listen_wrapper(void * a);
    void * gather_ipc_requests_wrapper(void * a);
  }
  
  ///\struct  _net_message_
  ///
  /// Network message structure
  ///\code
  /// std::string   dotted_ip;
  /// std::string   message;
  ///\endcode
  ///
  struct _net_message_ {
    std::string   dotted_ip;
    std::string   message;
  };
  typedef struct _net_message_ NetMessage_t;
  
  ///\struct  _process_id_
  ///
  //// Structure for server nominees and candidates
  ///\code
  /// std::string   ip_addr;
  /// pid_t         pid;
  /// bool          done;
  ///\endcode
  ///
  struct _process_id_ {
    std::string   ip_addr;
    pid_t         pid;
    bool          done;
  };
  typedef struct _process_id_ ProcessID_t;

  // callback for std::sort
  bool compare_pids (const ProcessID_t& first, const ProcessID_t& second);
  // callback for std::sort
  bool compare_addrs (const ProcessID_t& first, const ProcessID_t& second);

  ///\class ElectionManager
  /// The ElectionManager Class automatically detects servers running
  /// for a distribution and returns server's address.
  ///
  /// If no servers are found, but other ElectionManagers are running,
  /// the class will ''elect'' a server from among the candidates
  /// available.
  ///
  /// The class is a singleton and is not intended to be called
  /// directly from application code. It is embedded in the
  /// DistributedRunManager class for self-organizing client-server
  /// processes.
  
  class ElectionManager {
    
  public:
    /// ElectionManager constructor
    ///
    /// \param key - network ney for handshake
    /// \param port - election broadcast port
    ///
    ElectionManager(std::string key, int port);
    
    ~ElectionManager();
    
    /// Informational
    ///\return The server's IP address in a string
    ///
    std::string getServerIP() { return ServerIpAddress_; }
    
    /// Informational
    ///\return true if this process is the server process
    ///
    bool isServer()  { return IAmServer_; }
    
    // called from thread
    void * listen_to_broadcast();
    void * listen_to_ipc();
    void * gather_ipc_clients(int sock);

    void   CloseDown();
    
  private:
    // the result of our labors!
    std::string ServerIpAddress_;
    pid_t       ServerPID_;
    
    // state 
    bool  IPC_Client_Mode_;
    bool  ElectionCalled_;
    bool  IAmServer_;
    bool  ServerFound_;
    
    // strings used in protocol
    // careful of substring matches!
    const std::string ServerInquiry_  = "Inquiry";
    const std::string ServerResponse_ = "Responding";
    const std::string Nomination_     = "Nomination";
    const std::string NomAccepted_    = "Accepted";
    
    int         my_port_;
    std::string my_key_;
    pid_t       my_pid_;
    std::string my_ip_addr_;
    std::string loopback_addr_;
    
    int       broadcast_socket_;
    int       respond_socket_;
    int       listen_socket_;
    pthread_t listen_thread_;
    pthread_t ipc_listen_thread_;
    std::string ipc_socket_path_;
    
    //std::list<struct in_addr>  broadcast_addr_list_;
    std::list<struct ifreq>    broadcast_addr_list_;
    std::list<NetMessage_t>    r_message_list_;
    
    std::list<ProcessID_t> responder_list_;  // nodes on this channel
    std::list<ProcessID_t> candidate_list_;  // sorted nodes on this channel plus me
    std::list<ProcessID_t> nominee_list_;    // nodes above me
    std::list<ProcessID_t> nomrecv_list_;    // nodes who nominated me
    std::list<ProcessID_t> nomaccept_list_;  // nodes above me who answer
    std::list<ProcessID_t> server_list_;
    
    std::list<ipc_client_info_t>   ipc_client_list_;
    
    SyncLock r_message_lock_;
    SyncLock ipc_client_list_sync_;
    
    SyncLock Lis_Thread_Lock_;
    SyncLock Req_Thread_Lock_;
    
    std::string server_response_message_;
    
    void  make_inquiry();
    void  inquire_with_inet();
    void  inquire_with_ipc();
    
    void  open_listening();  
    void  open_ipc_listening();  
    
    void  parse_message_list();  
    void  put_message_in_list(NetMessage_t message, std::list<ProcessID_t> &list);
    
    int   broadcast_message(std::string& message);
    int   send_message(std::string ip_addr, std::string& message);
    
    void  elect_server();
    void  assign_server();
    int   server_responds();
    int   candidate_responds();
    
    std::string get_my_address();
    int   get_broadcast_addr();
    void *get_in_addr(struct sockaddr *sa);
    int   make_ioctl(int socket, int request, struct ifreq &the_ifreq);
    
    void  debug_print(std::string name,  std::list<ProcessID_t>list, const char * file, int line_no);
    
    int   read_socket(int sd, char * buf, int n);
    int   write_socket(int sd, char * buf, int n);
    
  };

};

#endif
