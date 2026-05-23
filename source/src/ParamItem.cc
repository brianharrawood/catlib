#include "ParamItem.hh"
#include "Exception.hh"

using namespace catlib;  

ParamItem::ParamItem(std::string new_line, std::map<std::string, ParamTemplate> tag_registry, FileIndex location):
  tag_(""),value_(""),data_type_(no_T),str_data_(""),int_data_(-1),flt_data_(-1.0),bool_data_(false),void_data_(NULL)
{  
  // fetch tagID and value
  parse_line(new_line);
  
  // retrieve tag_template from registry
  auto bad_tag_it = tag_registry.end();

  auto tag_template_it = tag_registry.find(tag_);
  
  if(tag_template_it == bad_tag_it){
    // exception - Illegal tag
    std::ostringstream stringStream;
    stringStream << "Illegal parameter found." << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }
  
  the_template_ = tag_template_it->second;
  
  data_type_ = the_template_.data_type;

  location_ = location;
  
  return;
}

ParamItem::ParamItem(std::string new_line, ParamTemplate tag_template, FileIndex location):
  tag_(""),value_(""),data_type_(no_T),str_data_(""),int_data_(-1),flt_data_(-1.0),bool_data_(false),void_data_(NULL)
{
  // fetch tagID and value
  parse_line(new_line);
  
  data_type_ = tag_template.data_type;

  location_ = location;
  
  the_template_ = tag_template;

  return;  
}

ParamItem::~ParamItem(){;}

void ParamItem::parse_line(std::string new_line)
{
  // split line at delimiter
  std::string delimiter = ":";
  
  tag_ = trim(new_line.substr(0, new_line.find(delimiter))); 
  
  if(tag_.empty()){
    // exception - Illegal tag
    std::ostringstream stringStream;
    stringStream << "No tagID found => expecting tagID: value" << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }
  
  // remove upto delimeter
  new_line.erase(0, new_line.find(delimiter) + delimiter.length());
  
  // get remaining line
  std::string the_rest = new_line.substr(0, new_line.find(delimiter)); 
  
  // remove any trailing comments
  delimiter = "#";
  value_   = trim(the_rest.substr(0, the_rest.find(delimiter))); 
  
  if(value_.empty()){
    // exception - Illegal tag
    std::ostringstream stringStream;
    stringStream << "No tag value found => expecting 'tagID: value'" << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }
  
}


std::string ParamItem::Serialize()
{
  return(tag_ + ": " + value_);
}

std::string ParamItem::trim(const std::string& str)
{

  std::string whitespaces (" \t\f\v\n\r");
  
  size_t first = str.find_first_not_of(whitespaces);

  if (std::string::npos == first)
    {
      return str;
    }
  size_t last = str.find_last_not_of(whitespaces);

  std::string r_string = str.substr(first, (last - first + 1));

  return r_string;
}

// overridded in derived classes
bool ParamItem::getData(void *) { return false; }
bool ParamItem::getData(int &data) { return false; }
bool ParamItem::getData(float &data) { return false; }
bool ParamItem::getData(bool &data) { return false; }
bool ParamItem::getData(std::string &data) { return false; }
