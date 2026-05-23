#include "TestInputParams.hh"
#include "TestParamTemplate.hh"

#include <string.h>

enum data_func_t {POINT_SPEC_T, UNKNOWN_TYPE};

struct user_tag_funcs
{
  std::string  tagID;
  data_func_t  tagFuncType;
} tag_func_list[] = {
  { std::string("PointSpecs"), POINT_SPEC_T}
};

// just for illustration purposes in the Test
void TestInputParams::load_user_param(std::string line_ptr, ParamTemplate tag_template, FileIndex &my_location)
{
  data_func_t data_func_type = UNKNOWN_TYPE;
  
  for(auto tag_func: tag_func_list){
    if(tag_template.tagID == tag_func.tagID){
      data_func_type =  tag_func.tagFuncType;
      break;
    }
  }
  
  switch(data_func_type){
  case POINT_SPEC_T:
    {
      load_point_spec(line_ptr);
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

int TestInputParams::load_point_spec(std::string spec)
{
  std::string input_tag = spec;

  // remove bounding brackets
  size_t pos = spec.find_first_of("{}");
  while(std::string::npos != pos){
    spec.erase(pos, 1);
    pos = spec.find_first_of("{}");
  }

  // parse tokens seperated by commas
  char * pch;
  pch = strtok ((char *)spec.c_str(), ",");
  std::string color = pch;
  pch = strtok (NULL, ",");
  std::string transparency = pch;
  pch = strtok (NULL, ",");
  std::string size = pch;

  PointSpec_t new_spec;

  new_spec.color  = color;

  try {
    new_spec.transparency = (float) std::stof(transparency);
  }
  catch (...) {
    // exception
    std::ostringstream stringStream;
    stringStream << "Failed converting transparency to float " << std::endl;
    stringStream << "input tag-> " << input_tag << std::endl;
    
    Exception_t my_e;
    my_e.message = stringStream.str();
    
    throw (my_e);
  }  
  
  try {
    new_spec.size = (int) std::stoi(transparency);
  }
  catch (...) {
    // exception
    std::ostringstream stringStream;
    stringStream << "Failed converting size to int " << std::endl;
    stringStream << "input tag-> " << input_tag << std::endl;
    
    Exception_t my_e;
    my_e.message = stringStream.str();
    
    throw (my_e);
  }  
  
  point_specs_.push_back(new_spec);
  
  return(0);
}

int TestInputParams::sanity_check()
{
  // app needs to report inconsistencies
  return(0);
}
