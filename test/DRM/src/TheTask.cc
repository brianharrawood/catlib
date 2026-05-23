#include "DRMtest.h"
// if osrelease < Ubuntu 24.04.4
//#include "bsd/stdlib.h"

// This is the Callback Function
// for each ticket number:
// this is where the calculations are performed 
void do_an_estimate(int ticket_number, void * arg)
{
  TestInputParams * inputParams = (TestInputParams *) arg;  

  std::string output_directory;
  float points_per_ticket = 0.0;
  
  inputParams->getParamData("output_dir", output_directory);
  inputParams->getParamData("PointsPerTicket", points_per_ticket);

  double x = 0;
  double y = 0;
  int    points_inside = 0;   // number of points inside the unit radius

  for(int i = 0; i < points_per_ticket; i++){

    // between (0,0) and (1,1)
    get_random_point(&x, &y);

    double radius = sqrt(x*x + y*y);

    if(radius <= 1)
      points_inside++;
  }

  // area ratio of 1/4 unit circle to unit square
  double pi_estimate = 4 * points_inside/(double)points_per_ticket;

  // write result file
  write_result(pi_estimate, ticket_number, output_directory);

  return;
}

void get_random_point(double *x, double *y)
{
  uint32_t upper_bound = 1000000000;

  // uniform random < upper_bound
  uint32_t xi = arc4random_uniform(upper_bound);
  *x = xi/(double)upper_bound;

  uint32_t yi = arc4random_uniform(upper_bound);
  *y = yi/(double)upper_bound;

}

void write_result(double pi_estimate, int ticket_number, std::string output_directory)
{
  // make sure we have a place for the file
  FileIO filer;

  std::string result_dir = output_directory + "/results";

  filer.make_dir(result_dir, 0755);

  char buffer[32];
  sprintf (buffer, "result_%04d.txt", ticket_number);
  
  std::string result_file = result_dir + "/" + std::string(buffer);

  // reuse buffer
  memset(buffer, 0, 32);
  sprintf (buffer, "pi estimate %1.6lf\n", pi_estimate);
  
  bool compress = false;
  filer.write_txt_file(result_file, buffer, compress);
}

// should write this to a file - but since this is just for testing, std::cout
void report_results(TestInputParams * inputParams, float seconds)
{
  namespace fs = std::filesystem;
  FileIO filer;

  std::string output_directory;
  float points_per_ticket = 0.0;
  int   number_of_tickets = 0.0;
  
  inputParams->getParamData("output_dir", output_directory);
  inputParams->getParamData("PointsPerTicket", points_per_ticket);
  inputParams->getParamData("NumberOfTickets", number_of_tickets);

  std::string result_dir = output_directory + "/results";
  char * buffer = 0;

  double epi;
  std::vector<double> estimate_vector;

  // collect esimates from result directory
  // sort the files so we don't open any extras from a previous run
  std::vector <std::string> result_files;

  for (const auto & entry : fs::directory_iterator(result_dir))
    result_files.push_back(entry.path());

  std::sort(result_files.begin(), result_files.end() );
  
  int file_index = 0;
  
  for(auto file_name : result_files){
    filer.read_txt_file(file_name, &buffer);
    sscanf (buffer,"%*s %*s %lf", &epi);
    estimate_vector.push_back(epi);
    delete [] buffer;

    file_index++;
    if(file_index >= number_of_tickets)
      break;
  }

  double sum = 0;
  
  for(double epi: estimate_vector)
    sum += epi;

  double average = sum / estimate_vector.size();
  long   total_points = estimate_vector.size() * points_per_ticket;
  float  kpoints = total_points/1000.0f;

  std::cout << std::endl;
  std::cout << "Number of tickets " << estimate_vector.size() << " each with " << points_per_ticket << " points." << std::endl;
  std::cout << "Runtime is " << seconds << " seconds (" << kpoints/seconds << " k-hist/sec)" << std::endl; 
    
  std::cout << "PI estimate is " << average << " in " << total_points << " total points." << std::endl;
  std::cout << "PI is " << M_PI << "; percent error is  " << 100 * abs(M_PI - average)/M_PI <<  std::endl;
  
}
