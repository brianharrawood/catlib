/**
 * \file DistributedRunManager.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for the DistributedRunManager.
 *
 */
#ifndef DistributedRunManager_hh
#define DistributedRunManager_hh

#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator> // for ostream_iterator

#include <list>
#include <vector>
#include <string>
#include <mutex>

// catlib
#include "TicketManager.hh"
#include "InputParams.hh"
#include "SyncLock.hh"
#include "ElectionManager.hh"
//#include "Log.hh"

// restart flags
#define READ_RESTART  1
#define WRITE_RESTART 2
      
// socket info
#define DEFAULT_PORT_NUMBER 0xdea8
#define S_BUFFER_LEN        1024
#define P_BUFFER_LEN        1024*16
      
#define CMD_LEN  64
#define HOST_LEN 256
#define PID_LEN  32
      
#define RESTART_FILE "./restart_DRM"
      
namespace catlib {
  
  ///\struct DRM_mode_t
  ///
  /// Enumerated run modes
  ///\code
  /// UNSPECIFIED,
  /// SERVER,
  /// CLIENT,
  ///\endcode
  enum DRM_mode_t {
    UNSPECIFIED = 0,
    SERVER,
    CLIENT
  };

  /// thread callback
  extern "C" void * wait_for_connection_wrapper(void * a);  
  /// thread callback
  extern "C" void * gather_requests_wrapper(void * a);

  ///\struct _client_info
  ///
  /// The DRM server's client info struct
  ///\code
  ///  pthread_t thread_id;
  ///  int       socket_id;
  /// \endcode
  ///
  struct _client_info {
    pthread_t thread_id;
    int       socket_id;
  };
  typedef struct _client_info client_info_t;
  
  class DistributedRunManager;
  
  ///\struct  extern_c_arg
  ///
  ///  The DRM struct to pass arguments in-and-out of class for
  ///  threading
  ///\code
  ///  DistributedRunManager * dis;
  ///  int                    sock;
  ///\endcode
  ///
  struct extern_c_arg {
    DistributedRunManager * dis;
    int                    sock;
  };
  typedef struct extern_c_arg Arg_t;

  /// \class DistributedRunManager
  /// DistributedRunManager (DRM) Class enables distribution of a
  /// parallel program to multiple processes/hosts. It runs as a
  /// singleton and requires the use of an InputParams class, a
  /// TicketManager Class and a Ticket class.
  ///
  /// The DRM uses a client-server model to handle the work
  /// flow. Exactly one server is needed for each disrtibution
  /// run. The server process may be specified in the class
  /// constructor or the server my be elected at run time using the
  /// ElectionManager Class in the election constructor.
  ///
  /// The DRM Callback, called from RunTickets() is a pointer to <void
  /// function(int, void*)> declared in TicketManager.hh typedef
  /// void(\*DRMCallbackFunc_t)(int, void\*); The application is
  /// required to register a TicketManager before it can process any
  /// tickets.
  //
  class DistributedRunManager
  {
  public:

    /// DRM mode selected at runtime by ElectionManager; port=0 will use default port
    ///
    /// \param input - pointer to InputParams
    /// \param use_restart - true if restarting from restart file
    /// \param key - network ney for handshake
    /// \param port - server listening port
    ///
    DistributedRunManager(InputParams * input, bool use_restart, std::string key, int port = 0);

    /// DRM starts as server process; port=0 will use default port
    ///
    /// \param mode = catlib::SERVER
    /// \param input - pointer to InputParams
    /// \param use_restart - true if restarting from restart file
    /// \param key - network ney for handshake
    /// \param port - server listening port
    ///
    DistributedRunManager(DRM_mode_t mode, InputParams * input, bool use_restart, std::string key, int port = 0);
    
    /// DRM starts as client process; port=0 will use default port
    ///
    /// \param mode = catlib::CLIENT
    /// \param input - pointer to InputParams
    /// \param use_restart - true if restarting from restart file
    /// \param server - host name of the server
    /// \param key - network key for handshake
    /// \param port - server listening port
    ///
    DistributedRunManager(DRM_mode_t mode, InputParams * input, bool use_restart, std::string server, std::string key, int port = 0);
    
    /// DRM is a singleton  
    /// Constructors which copy/move/clone have been DELETED
    DistributedRunManager(DistributedRunManager &other) = delete;
    void operator=(const DistributedRunManager &) = delete;
        
    ~DistributedRunManager();
    
  public:

    /// set TicketManager - REQUIRED to enable the DRM to keep track
    /// of tickets
    /// 
    /// \param ticket_manager is ticket manager class for this DRM
    ///
    void   RegisterTicketManager(TicketManager * ticket_manager)
    {
      user_ticket_manager_   = ticket_manager;
      // allow next_ticket requests
      // in handle_requests thread
      NextTck_Lock_.Unlock();
    }

    /// Start the analysis
    ///\return 0 on success - throws exceptions
    ///
    /// The call to RunTickets begins the looping over tickets. The
    /// call returns when all tickets have been completed.
    int    RunTickets();

    /// Increase verbosity
    ///
    /// Set v = 1 for modestly increased verbosity
    void   SetVerbose(int v) { Verbose_ = v; }

    /// Informational
    ///\return true if server
    ///
    /// This permits the calling application to determine if it is
    /// running as a server or client.
    bool   IsServer() { return ((mode_ == SERVER) ? true : false); }
    
    //  handle unix SIGINT (^c)
    /// public method for signal interface ONLY
    void   SignalHandler (int signum);

    /// public method for signal interface ONLY
    static void static_SignalHandler(int signum)
    {
      DistributedRunManager * DRM = DistributedRunManager::drm_instance_;

      if(DRM != nullptr)
	DRM->SignalHandler(signum);
      else
	// should not happen
	exit(EXIT_FAILURE);
    }
    
    /// public method for thread interface ONLY
    void * wait_for_connection(int sock);
    /// public method for thread interface ONLY
    void * gather_requests(int sock);
    
  private:
    // weak implementation of 'singleton' since constructor is still public
    // needed for signal handling 
    static DistributedRunManager * drm_instance_;
    static std::mutex drm_mutex_;
    
    // common methods
    void   zero_it();
    void   make_handshake_string();
    void   make_process_string();
    // client and server both this
    int    init_dispatch();

    // server methods
    void   run_server();
    int    start_listening();  // returns a socket
    int    close_dispatch();
    void   wait_for_disconnect();
    void   thread_accept(int sock);
    int    GetNextTicket(int mode, Ticket * ticket, char * clientInfo = 0);    
    int    safely_get_ticket(Ticket * ticket, char * info);
    void   provide_handshake(char * buffer, int sock);
    void   provide_next_ticket(char * buffer, int sock);
    void   provide_parameters(char * buffer, int sock);

    // server logging methods
    void   init_logging(std::string dir);
    void   assign_cout(Ticket * the_ticket, std::ofstream *filestr, std::streambuf ** backup);
    void   return_cout(std::ofstream * filestr, std::streambuf ** backup);
    void   assign_stdout(Ticket * the_ticket);
    
    // client methods
    void   run_client();
    int    client_connect_to_server(std::string host);
    int    client_request_handshake(int sock);
    int    client_request_parameters(int sock);
    int    client_request_next_ticket(int sock, Ticket * the_ticket);

    // communication methods
    int    read_socket(int sd, char * buf, int n);
    int    write_socket(int sd, char * buf, int n);
    void   block_signals();
    void   unblock_signals();
    void   mark_as_finished(Ticket * ticket);
    int    position_is_finished(int position);
    int    check_if_finished(Ticket * ticket);
    int    handle_restart_file(Ticket * ticket, int restart_mode);
    void   mv_restart_file();
    
    void   PackNetworkTicket(char * buffer, char * command, char * client_host, char * pid, Ticket * tx_ticket);
    void   UnpackNetworkTicket(char * buffer, char * command, char * client_host, char * pid, Ticket *  tx_ticket);
    
    InputParams     * inputParams_;
    ElectionManager * election_manager_;
    
    DRM_mode_t      mode_;
    int             server_port_;
    std::string     server_name_; // ip or host name
    
    std::string     current_output_name_;
    
    std::string     HandshakeString_;
    std::string     NetKey_; 
    std::string     ProcessString_;
    
    FILE          * RESTART_read_stream_;
    FILE          * RESTART_write_stream_;
    
    DRMCallbackFunc_t  RunTicketsCallback_;
    void             * RunTicketsCallbackArg_;
    int                number_of_tickets_;
    
    // synchronization
    catlib::SyncLock  GatherReq_Lock_; // to start gather_request
    catlib::SyncLock  FD_Lock_;        // file descriptor lock
    catlib::SyncLock  CL_Lock_;        // client list lock
    catlib::SyncLock  Ticket_Lock_;    // get ticket lock
    catlib::SyncLock  Restart_Lock_;   // restart ticket lock
    catlib::SyncLock  Parameter_Lock_; // share parameters with clients
    catlib::SyncLock  HandShake_Lock_; // provide_handshake
    catlib::SyncLock  NextTck_Lock_;   // provide next_ticket
    
    pthread_t         listen_thread_;
    int               listen_socket_;
    int               client_socket_;
    
    std::list<client_info_t> client_list_;    // list of client threads in server
    
    TicketManager   * user_ticket_manager_;
    std::string       log_dir_;
    
    // flags
    int    Verbose_;
    int    RestartFlag_;  // 0 skip restart file, 1 use restart file, 2 use restart to check run at close

  };

};

#endif
