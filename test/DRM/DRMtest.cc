#include "DRMtest.h"
#include <memory>

// usage
// ./DRM_test -f <parameter_file> [-p <port_number>] [-r]
int main(int argc, char ** argv)
{
  cmd_input command_line;
  if(-1 == parse_command_line(argc, argv, command_line)){
    usage();
    exit(EXIT_FAILURE);
  }

  try {
    // gather input
    int  port              = command_line.port_number;
    bool restart_prior_run = command_line.restart_flag;
    
    TestInputParams inputParams(DRMtest_templates);
    
    inputParams.loadParams(command_line.parameter_file);
    
    std::string  net_key = "cwd failed!";
    char  cwd[PATH_MAX];
    
    if(NULL != getcwd(cwd, PATH_MAX))
      net_key = cwd;
    
    // initialize the run manager
    //
    
    // broadcast on network for other processes started in this
    // directory (i.e., same net_key, same port).  If a server process
    // answers, connect as client.  If multiple processes answer, but no
    // servers are present, elect a server and connect as client.  If no
    // other process answer, start as a server.
    //
    DistributedRunManager DRM(&inputParams, restart_prior_run, net_key, port);
    
    // set up the ticket management
    //
    
    // the function which processes each ticket
    catlib::DRMCallbackFunc_t callback_func = do_an_estimate;
    
    // the ticket manager tracks which ticket is next
    // and when the job is done
    TestTicketManager myTicketManager(callback_func, &inputParams);
    
    DRM.RegisterTicketManager(&myTicketManager);
    
    // start clock here to avoid latency of start-up
    clock_t t = clock();
    
    // run the tickets - the work happens here
    //
    DRM.RunTickets();
    
    t = clock() - t;
    float elapsed_seconds = t/(float)CLOCKS_PER_SEC;
    
    // collate results and print average
    if(DRM.IsServer()) {
      // print the parameters
      inputParams.Dump();
      report_results(&inputParams, elapsed_seconds);
    }
  }

  catch (catlib::Exception_t& e){
    std::cerr << e.message;
    std::cerr << "Exiting...\n";
    return(-1);
  }
  
  return(0);
}


#define ARGS "f:p:rh?vV"

int parse_command_line(int ac, char ** av, CmdLineOptions_t &cmd_options)
{
  int    opt;
  char * file_name = 0;
  char * host_name = 0;
  
  // default condition
  cmd_options.port_number  = 0;
  cmd_options.restart_flag = false;

  // fresh start
  optind = 0;

  while(1){
    int option_index = 0;
    static struct option long_options[] = {
      {0, 0, 0, 0}
    };

    opt = getopt_long (ac, av, ARGS, long_options, &option_index);
    
    if (opt == -1)
      break;
    
    switch (opt) {
      
    case 'f':
      file_name = new char[strlen(optarg) + 1];
      if(!file_name){
	std::cerr << "memory exhausted " << __FILE__ << ":" << __LINE__ << std::endl;
	return(-1);
      }
      strcpy(file_name, optarg);
      // copy to std::string
      cmd_options.parameter_file = file_name;
      delete[] file_name;
      file_name = 0;
      break;     
      
    case 'p':
      if(1 != sscanf(optarg, "%d", &cmd_options.port_number)){
	std::cerr << "Error scanning -p option" << std::endl;
	return(-1);
      } 
      break;     

    case 'r':
      cmd_options.restart_flag = true;
      break;     

    case '?':
    case 'h':
      usage();
      exit(EXIT_FAILURE);
      break;     

    default:
      std::cerr << "Unrecognized flag: '" << opt << "'" << std::endl;
      return(-1);
    }    
  }

  if(cmd_options.parameter_file.size() == 0){
    std::cerr << "Param file must be specified!" << std::endl;
    return(-1);
  }
  
  return(0);
}

void usage()
{
  std::cerr << "To run as distributed process (or singleton)" << std::endl;
  std::cerr << "\t DRM_test -f <param_file> [-p <port>] [-r]" << std::endl;
  std::cerr << "Where:" << std::endl;
  std::cerr << "\t <param_file> is the path to the parameter file," << std::endl;
  std::cerr << "\t <port> is the network port (optional, one is provided by default)" << std::endl;
  std::cerr << "\t (the port numbers must be the same for each process in group)," << std::endl;
  std::cerr << "\t -r examine DRM_restart file for completed tickets to skip" << std::endl;
}
  
