#include "InputParams.hh"
#include "Exception.hh"

#include "include/myParams.hh"
#include "include/myParamTemplate.hh"

#include <string>

using namespace catlib;

int main(int argc, char ** argv)
{

  myParams * the_params = new myParams(my_templates);
  
  std::string file_name = "input/params.txt";
  
  try {
    the_params->loadParams(file_name);
    
    the_params->Dump();
    
  }
  catch (Exception e) {
    std::cerr << e.message;
    exit(EXIT_FAILURE);
  }

  std::vector<Voter_t> the_voters = the_params->getVoters();

  int num_registered = 0;

  for(auto voter: the_voters){
    if(voter.registered)
      num_registered++;
  }

  std::cout << "Out of " << the_voters.size() << " voters, the number registered is " << num_registered << std::endl;

  int num_points = 0;
  the_params->getParamData("NumberOfPoints", num_points);

  float point_on_line = 0;
  the_params->getParamData("PointOnLine", point_on_line);
  
  std::cout << "There are " << num_points << " points on line and one of them is " << point_on_line << std::endl;
  
  
  delete the_params;
  
  return(0);
}
