#include "myParams.hh"
#include "myParamTemplate.hh"

#include <string.h>

enum data_func_t {NAMES_T, VOTER_T, UNKNOWN_TYPE};

struct user_tag_funcs
{
  std::string  tagID;
  data_func_t  tagFuncType;
} tag_func_list[] = {
  { std::string("Names"), NAMES_T},
  { std::string("Voter"), VOTER_T},
};

// virtual in base class
// just for illustration purposes in the Test
void myParams::load_user_param(std::string line_ptr, ParamTemplate tag_template, FileIndex &my_location)
{
  data_func_t data_func_type = UNKNOWN_TYPE;
  
  for(auto tag_func: tag_func_list){
    if(tag_template.tagID == tag_func.tagID){
      data_func_type =  tag_func.tagFuncType;
      break;
    }
  }
  
  switch(data_func_type){
  case NAMES_T:
    {
      load_names(line_ptr);
      break;
    }
  case VOTER_T:
    {
      load_voters(line_ptr);
      break;
    }
  case UNKNOWN_TYPE:
  default:
    {
      // exception
      std::ostringstream stringStream;
      stringStream << "Tag '" << tag_template.tagID << "' not loaded correctly!!" << std::endl;
      stringStream << "input tag-> " << line_ptr << std::endl;
      
      Exception_t my_e;
      my_e.message = stringStream.str();
      
      throw (my_e);
    }
  }

  return;
}

// inadequate error checking!
// expects {label, density, file_name}
int myParams::load_names(std::string spec)
{
  the_names_.push_back(spec);
  
  return(0);
}

// inadequate error checking!
// expects {name, registered, party, county}
int myParams::load_voters(std::string spec)
{
  std::string input = spec;
  
  // remove bounding brackets
  size_t pos = spec.find_first_of("{}");
  while(std::string::npos != pos){
    spec.erase(pos, 1);
    pos = spec.find_first_of("{}");
  }

  // parse tokens seperated by commas
  char * pch;
  pch = strtok ((char *)spec.c_str(), ",");
  std::string name = pch;
  pch = strtok (NULL, ",");
  std::string registered = pch;
  pch = strtok (NULL, ",");
  std::string affiliation = pch;
  pch = strtok (NULL, ",");
  std::string county = pch;

  Voter_t new_voter;

  new_voter.name = name;

  transform(registered.begin(), registered.end(), registered.begin(), ::tolower);
  if(registered.find("yes") != std::string::npos)
    new_voter.registered = true;
  else
    new_voter.registered = false;

  if(affiliation.find("D")!= std::string::npos)
    new_voter.party = D;
  else if(affiliation.find("R")!= std::string::npos)
    new_voter.party = R;
  else if(affiliation.find("I")!= std::string::npos)
    new_voter.party = I;
  else if(affiliation.find("U")!= std::string::npos)
    new_voter.party = U;
  else {
    // exception
    std::ostringstream stringStream;
    stringStream << "input tag-> " << input << std::endl;
    stringStream << "Has unknown voter affiliation: " << std::endl;
    stringStream << "Expecting one of D, R, I, or U. " << std::endl;
    
    Exception_t my_e;
    my_e.message = stringStream.str();
    
    throw (my_e);
  }

  new_voter.county = county;


  the_voters_.push_back(new_voter);
  
  return(0);
}

int myParams::sanity_check()
{
  // app needs to report inconsistencies
  return(0);
}
