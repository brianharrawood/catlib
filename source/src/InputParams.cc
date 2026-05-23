#include "InputParams.hh"
#include "Exception.hh"

#include "ParamItem.hh"
#include "FileIO.hh"

using namespace catlib;
  
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sstream>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream> 
#include <string>
#include <cstring>
#include <regex>

#include "ShaHash.h"

InputParams::InputParams(std::vector<ParamTemplate> the_templates):ParamsLoaded_(false)
{
  param_map_.clear();
  user_templates_ = the_templates;
}

InputParams::~InputParams()
{
  for(auto map_pair: param_map_)
    delete map_pair.second;

  for(auto p_item: param_vec_)
    delete p_item;

}

void InputParams::loadParams(std::string &param_file)
{
  // you can only load one file with this method
  // included files use different path through the class
  if(ParamsLoaded_)
    return;
  
  // load in the tag 'library'
  loadRegistry();
  
  char * param_buffer;
  
  // first param file
  if(master_file_name_.length() == 0){
    master_file_name_ = param_file;
    current_file_index_.file_name = param_file;
  }

  // parsing parameters into param_map_ (or param_vec_)
  try {
    // read file into memory
    loadParamsFile(param_file);
  }
  catch (Exception_t& e) {
    // exception
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Failed parsing file: '" << param_file << "'." << std::endl;
    
    Exception_t new_e;
    new_e.message = stringStream.str();
    
    throw (new_e);
  }
  
  // 
  CheckSet("output_dir", output_dir_, "./");

  catlib::FileIO filer;
  
  filer.make_dir(output_dir_, 0755);
  
  ParamsLoaded_ = true;
  
}

// maybe add additional parameters on the fly
void InputParams::loadOverrideParams(std::string &extra_params)
{
  if(extra_params.empty())
    return;

  bool over_ride = true;
  
  FileIndex location;
  location.file_name = std::string("command line");
  location.line_number = 0;

  std::istringstream extras = std::istringstream(extra_params);

  for (std::string line; std::getline(extras, line);){
    // make this just to use  void_param.getTagID() -> to get template
    ParamItemVoid void_param = ParamItemVoid(line, InputParams::tag_registry_, location);
    ParamTemplate tag_template = (InputParams::tag_registry_[void_param.getTagID()]);
    
    // this is for including other tag files
    if(tag_template.tag_type == ctl_T)
      // no way!
      continue;
    else    
      load_app_param(line, tag_template, location.line_number, over_ride);  
  }

  //  may create a different directory than loadParams() 
  CheckSet("output_dir", output_dir_, "./");

  catlib::FileIO filer;
  
  filer.make_dir(output_dir_, 0755);
  
}

void InputParams::loadParamsFile(std::string param_file)
{
  catlib::FileIO filer;
  char         * param_buffer;
  
  // parsing parameters into param_map_ (or param_vec_)
  try {
    // read file into memory
    filer.read_txt_file(param_file, &param_buffer);
    
    // parse file into parameters
    // and fill param_map_
    parse_param_buffer(param_buffer);

    delete[] param_buffer;
  }
  catch (Exception_t& e) {
    // exception
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Failed parsing file: '" << param_file << "'." << std::endl;
    
    Exception_t new_e;
    new_e.message = stringStream.str();
    
    throw (new_e);
  }

}

int InputParams::parse_param_buffer(char * param_buffer)
{
  // parse the buffer
  char * buf_ptr = param_buffer;
  char * line_ptr = 0;
  char * nl;
  char * tok;
  int    line_no  = 0;

  do {
    // get next_line
    nl = strchr(buf_ptr, '\n');
    
    if(nl)
      *nl = '\0';
    
    line_no++;
    
    line_ptr = buf_ptr;
    
    if(nl)
      buf_ptr = nl + 1;

    else {
      // exception - Not sure what's wrong
      std::ostringstream stringStream;
      stringStream << "Missing new line at end of file. ";
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
    
    // parse the line
    if(line_ptr[0] == '#'){
      // comment
      continue;
    }

    if(std::string(line_ptr).find_first_not_of(' ') == std::string::npos) {
      // blank line
      continue;
    }
    
    // line_ptr should be a tag string => tag: value
    try {

      map_tag(std::string(line_ptr), line_no);

    }

    catch (Exception_t& e) {
      // illegal tag
      // in case we are recursing
      if(std::string::npos != e.message.find("BadTag:"))
	throw(e);
      if(std::string::npos != e.message.find("DuplicateTag:"))
	throw(e);

      std::ostringstream stringStream;
      stringStream << e.message;

      FileIndex tag_location;
      tag_location.file_name = current_file_index_.file_name;
      tag_location.line_number = line_no;
      
      std::vector<FileIndex> this_location;
      if(!included_files_.empty())
	this_location = included_files_;
      
      this_location.push_back(tag_location);
      
      auto loc_itr = this_location.begin();
      for(;loc_itr != this_location.end(); loc_itr++)
	stringStream << "\t" << loc_itr->file_name << " at line " << loc_itr->line_number << std::endl;
      
      Exception_t new_e;
      new_e.message = stringStream.str();
      
      throw (new_e);
    }
    
  } while(*buf_ptr); // end do - while
  

  return(0);
}

void InputParams::map_tag(std::string line, int line_no)
{
  FileIndex location;
  
  // make this just to use  void_param.getTagID() -> to get template
  ParamItemVoid void_param = ParamItemVoid(line, InputParams::tag_registry_, location);
  ParamTemplate tag_template = (InputParams::tag_registry_[void_param.getTagID()]);

  // this is for including other tag files
  if(tag_template.tag_type == ctl_T)
    load_ctl_param(line, tag_template, line_no);
  else
    load_app_param(line, tag_template, line_no);  
}

// handles control params - just the 'include' param for now
void InputParams::load_ctl_param(std::string line_ptr, ParamTemplate tag_template, int line_no)
{
  IncludeRecursion_ += 1;

  FileIndex my_location;
  my_location.file_name = current_file_index_.file_name;
  my_location.line_number = line_no;

  // only one ctl_T tag at the moment => include
  // get tag value
  ParamItemStr *include_param = new ParamItemStr(line_ptr, tag_template, my_location);

  std::string include_file;
  bool rc = include_param->getData(include_file);

  if(rc == false){
    std::ostringstream stringStream;
    stringStream << "Bad 'include' tag" << std::endl;    
    stringStream << current_file_index_.file_name << ":" << line_no << std::endl;
    Exception_t e;
    e.message = stringStream.str();
    throw (e);
  }

  // check number of recursions
  if(IncludeRecursion_ > INCLUDE_RECURSION_MAX){
    std::ostringstream stringStream;
    stringStream << "Maximum recursion of parameter files (" << INCLUDE_RECURSION_MAX << ") has been exceeded!" << std::endl;
    stringStream << "Check for looping 'include' statements" << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }
   
  //
  // track the include file stack
  included_files_.push_back(my_location);

  // save name of previous file
  std::string calling_file = current_file_index_.file_name;  
  current_file_index_.file_name = include_file;

  // map the tags
  loadParamsFile(include_file);
  
  // pop this file off of included_file_name_ vector
  included_files_.pop_back();
  current_file_index_.file_name = calling_file;

}

// creates the param_map
void InputParams::load_app_param(std::string line_ptr, ParamTemplate tag_template, int line_no, bool over_ride)
{
  FileIndex my_location;
  if(over_ride){
    my_location.file_name = "command line override";
    my_location.line_number = 0;
  }
  else {
    my_location.file_name = current_file_index_.file_name;
    my_location.line_number = line_no;

  }
  std::vector<FileIndex> location_vec;
  if(!included_files_.empty())
    location_vec = included_files_;

  location_vec.push_back(my_location);

  if((!tag_template.unique) && (tag_template.data_type != user_T)){
      // exception
      std::ostringstream stringStream;
      
      stringStream << "BadTag: '" << tag_template.tagID << "'" << std::endl;
      stringStream << "This tag is coded to be allowed multiple times in the parameter file, " << std::endl;
      stringStream << "therefore it must use a unique application data type." << std::endl;
      
      Exception_t new_e;
      new_e.message = stringStream.str();
      
      throw (new_e);
  }
  
  // duplicate tag check
  if(tag_template.unique && (over_ride == false)){

    auto map_itr = param_map_.find(tag_template.tagID);
    
    if(map_itr != param_map_.end()){
      // exception
      std::ostringstream stringStream;
      
      stringStream << "DuplicateTag: '" << tag_template.tagID << "'" << std::endl;
      
      auto loc_itr= location_vec.begin();
      for(;loc_itr != location_vec.end(); loc_itr++)
	stringStream << "\t" << current_file_index_.file_name << " at line " << loc_itr->line_number << std::endl;
      
      auto orig_location = param_location_.find(tag_template.tagID);
      auto orig_tag_location = orig_location->second;
      
      stringStream << "Also set in: " << std::endl;
      std::vector<FileIndex>::reverse_iterator orig_loc_itr = orig_tag_location.rbegin();
      
      for(;orig_loc_itr != orig_tag_location.rend(); orig_loc_itr++)
	stringStream << "\t'" << orig_loc_itr->file_name << "' at line " << orig_loc_itr->line_number << std::endl;
      
      Exception_t new_e;
      new_e.message = stringStream.str();
      
      throw (new_e);
    }
  }

  // over riding user type tags is undefined the override would need
  // to be handled in the virtual function load_user_param()
  if(over_ride && (tag_template.data_type == user_T)){
    // hmmm 
    std::ostringstream stringStream;
    
    stringStream << "Overriding the tag '" << tag_template.tagID << "' will result in undefined results!" << std::endl;
    
    Exception_t new_e;
    new_e.message = stringStream.str();
    
    throw (new_e);
  }

  // if over riding a previous param, it must be removed from the map
  // before it can be re-inserted
  if(over_ride){
    auto map_itr = param_map_.find(tag_template.tagID);
    param_map_.erase(map_itr);
  }
  
  // put it in the param_map
  // or store in a list (non-unique tags)
  switch(tag_template.data_type){
  case string_T:
    {
      ParamItemStr *str_param = new ParamItemStr(line_ptr, tag_template, my_location);
      
      std::pair<std::string, ParamItemStr*> param_pair(str_param->getTagID(), str_param);
      param_map_.insert(param_pair);
      break;
    }
  case int_T:
    {
      ParamItemInt *int_param = new ParamItemInt(line_ptr, tag_template, my_location);

      std::pair<std::string, ParamItemInt*> param_pair(int_param->getTagID(), int_param);
      param_map_.insert(param_pair);
      break;
    }
  case float_T:
    {
      ParamItemFlt *flt_param = new ParamItemFlt(line_ptr, tag_template, my_location);

      std::pair<std::string, ParamItemFlt*> param_pair(flt_param->getTagID(), flt_param);
      param_map_.insert(param_pair);
      break;
    }
  case bool_T:
    {
      ParamItemBool *bool_param = new ParamItemBool(line_ptr, tag_template, my_location);

      std::pair<std::string, ParamItemBool*> param_pair(bool_param->getTagID(), bool_param);
      param_map_.insert(param_pair);
      break;
    }
  case user_T:
    {
      ParamItemVoid *void_param = new ParamItemVoid(line_ptr, tag_template, my_location);

      load_user_param(void_param->getValue(), tag_template, my_location);

      param_vec_.push_back(void_param);

      break;
    }
  }
  
  // remember where we read this tag  
  std::pair<std::string, std::vector<FileIndex>> param_pair(tag_template.tagID, location_vec);
  param_location_.insert(param_pair);
}

// virtual method to load a user specified data type into the
// param_map
void InputParams::load_user_param(std::string line_ptr, ParamTemplate tag_template, FileIndex &my_location)
{
  std::cerr << "Tag '" << tag_template.tagID << "' is not being loaded!!" << std::endl;
}

// virtual method to check param consistency
int InputParams::sanity_check()
{
  // app needs to report inconsistencies
  return(0);
}

char * InputParams::get_token(char * line, const char * token)
{
  int i = 0;

  while((line[i] == token[i]) && (line[i] != '\0') && (token[i] != '\0'))
    i++;

  if(i != (signed)strlen(token))
    // no match
    return(NULL);
    
  // eat white space
  while(isspace(line[i]) && (line[i] != '\0'))
    i++;

  if(line[i] == '\0')
    return(NULL);

  int j = i;

  // if there is a '#' sign or extra whitespace, end line there
  while((line[j] != '\0') && (line[j] != '#') && (!isspace(line[j])))
    j++;

  line[j] = '\0';

  return(&line[i]);
}


// write all parameters to character stream
void  InputParams::Serialize(std::stringstream &ss)
{
  if(ParamsLoaded_ == false)
    // not loaded yet
    return;
  
  for(auto map_iter: param_map_){
    ss << map_iter.second->Serialize();
    ss << std::endl;
  }
  
  for(auto vec_iter: param_vec_){
    ss << vec_iter->Serialize();
    ss << std::endl;
  }

}

// unused
// read parameters from character stream
void  InputParams::Unserialize(std::stringstream &ss)
{
  // you can only load one file with this object
  // included files use different path through the class
  if(ParamsLoaded_)
    return;
  
  // load in the tag 'library'
  loadRegistry();

  // serialized params
  if(master_file_name_.length() == 0){
    master_file_name_ = "serialized";
    current_file_index_.file_name = "serialized";
  }
  
  std::string param_string = ss.str();
  
  // parse buffer into tokens
  try {
    // parse file into parameters
    parse_param_buffer((char *)param_string.c_str());
  }
  catch (Exception_t& e) {
    // exception
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Failed serialize/unserializing parameters." << std::endl;
    
    Exception_t new_e;
    new_e.message = stringStream.str();
    
    throw (new_e);
  }

  ParamsLoaded_ = true;

}

// write parameters to output file
void InputParams::Dump()
{
  if(master_file_name_.size() == 0)
    // not ready
    return;
  
  char dump_file_name[1024];
  
  int pid = (int)getpid();

  sprintf(dump_file_name, "%s/parameters_%d.txt", output_dir_.c_str(), pid);
  
  FILE * dump_file = fopen(dump_file_name, "w+");
  
  if(!dump_file){
    std::cerr << "File open failed " << __FILE__ << ":" << __LINE__ <<  std::endl;
    std::cerr << strerror(errno) << " for parameter dump file " << dump_file_name  << std::endl;
    return;
  }
  
  fprintf(dump_file, "# Parameters loaded from '%s'\n\n", master_file_name_.c_str());

  if(serialized_params_.rdbuf()->in_avail() == 0)
    Serialize(serialized_params_);

  fprintf(dump_file, "%s", serialized_params_.str().c_str());
  
  fclose(dump_file);
}

std::string InputParams::getParamKey()
{
  if(serialized_params_.rdbuf()->in_avail() == 0)
    Serialize(serialized_params_);

  if(ParamKey_.empty()){
    char key[65];
    const char * params = serialized_params_.str().c_str();
    int rc = hash_string((char *)params, key);
    
    ParamKey_ = key;
  }
  
  return(ParamKey_);
}


void InputParams::loadRegistry()
{
  // prevent double loading
  if(!InputParams::tag_registry_.empty())
    return;

  for(auto a_template: the_templates){

    char * the_id = (char *)a_template.tagID;

    tag_registry_[the_id] = a_template;
  }

  for(auto a_template: user_templates_){
    
    char * the_id = (char *)a_template.tagID;
    
    tag_registry_[the_id] = a_template;
  }
  
}

//
bool InputParams::isParamSet(const char* tag_id)
{
  auto map_itr = param_map_.find(std::string(tag_id));

  if(map_itr != param_map_.end())
    return true;

  for(auto vec_itr: param_vec_){
    if(std::string(tag_id) == vec_itr->getTagID())
      return true;
  }
  
  return false;
}

//
int InputParams::getParamData(std::string tag, std::string &param_data)
{
  auto map_itr = param_map_.find(tag);

  if(map_itr == param_map_.end()){
    // not found exception
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << "Parameter '" << tag << ":' not found!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }

  ParamItemStr * str_param = (ParamItemStr*)map_itr->second;

  if(str_param->getType() != string_T){
    // wrong type exception
    std::string tag_type;
    switch(str_param->getType()){
    case string_T:
      tag_type = "string";
      break;
    case int_T:
      tag_type = "int";
      break;
    case float_T:
      tag_type = "float";
      break;
    case bool_T:
      tag_type = "bool";
      break;
    case user_T:
      tag_type = "app specific";
      break;
    case no_T:
      tag_type = "unknown";
      break;
    }
    
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Coding error calling getParamData() " << std::endl;
    stringStream << "tag " << tag << " type is " << tag_type << std::endl;
    stringStream << "tag " << tag << " CANNOT be set to float!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }

  if(false == str_param->getData(param_data)){
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Failed to set data for tag " << tag <<  std::endl;

    e.message = stringStream.str();
    throw (e);
  }
  
  return(0);
}

int InputParams::getParamData(std::string tag, int &param_data)
{
  auto map_itr = param_map_.find(tag);

  if(map_itr == param_map_.end()){
    // not found exception
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << "Parameter '" << tag << ":' not found!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }

  ParamItemInt * int_param = (ParamItemInt*)map_itr->second;

  if(int_param->getType() != int_T){
    // wrong type exception
    std::string tag_type;
    switch(int_param->getType()){
    case string_T:
      tag_type = "string";
      break;
    case int_T:
      tag_type = "int";
      break;
    case float_T:
      tag_type = "float";
      break;
    case bool_T:
      tag_type = "bool";
      break;
    case user_T:
      tag_type = "app specific";
      break;
    case no_T:
      tag_type = "unknown";
      break;
    }
    
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Coding error calling getParamData() " << std::endl;
    stringStream << "tag " << tag << " type is " << tag_type << std::endl;
    stringStream << "tag " << tag << " CANNOT be set to float!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }

  if(false == int_param->getData(param_data)){
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Failed to set data for tag " << tag <<  std::endl;

    e.message = stringStream.str();
    throw (e);
  }
  
  return(0);
}

int InputParams::getParamData(std::string tag, float &param_data)
{
  auto map_itr = param_map_.find(tag);

  if(map_itr == param_map_.end()){
    // not found exception
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << "Parameter '" << tag << ":' not found!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }

  ParamItemFlt * flt_param = (ParamItemFlt*)map_itr->second;

  if(flt_param->getType() != float_T){
    // wrong type exception
    std::string tag_type;
    switch(flt_param->getType()){
    case string_T:
      tag_type = "string";
      break;
    case int_T:
      tag_type = "int";
      break;
    case float_T:
      tag_type = "float";
      break;
    case bool_T:
      tag_type = "bool";
      break;
    case user_T:
      tag_type = "app specific";
      break;
    case no_T:
      tag_type = "unknown";
      break;      
    }
    
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Coding error calling getParamData() " << std::endl;
    stringStream << "tag " << tag << " type is " << tag_type << std::endl;
    stringStream << "tag " << tag << " CANNOT be set to float!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }
  
  if(false == flt_param->getData(param_data)){
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Failed to set data for tag " << tag <<  std::endl;

    e.message = stringStream.str();
    throw (e);
  }
  
  return(0);
}

int InputParams::getParamData(std::string tag, bool &param_data)
{
  auto map_itr = param_map_.find(tag);

  if(map_itr == param_map_.end()){
    // not found exception
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << "Parameter '" << tag << ":' not found!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }

  ParamItemBool * bool_param = (ParamItemBool*)map_itr->second;

  if(bool_param->getType() != bool_T){
    // wrong type exception
    std::string tag_type;
    switch(bool_param->getType()){
    case string_T:
      tag_type = "string";
      break;
    case int_T:
      tag_type = "int";
      break;
    case float_T:
      tag_type = "float";
      break;
    case bool_T:
      tag_type = "bool";
      break;
    case user_T:
      tag_type = "app specific";
      break;
    case no_T:
      tag_type = "unknown";
      break;
    }
    
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Coding error calling getParamData() " << std::endl;
    stringStream << "tag " << tag << " type is " << tag_type << std::endl;
    stringStream << "tag " << tag << " CANNOT be set to float!" << std::endl;
    e.message = stringStream.str();
    throw (e);
  }

  if(false == bool_param->getData(param_data)){
    Exception_t e;
    std::ostringstream stringStream;
    stringStream << e.message;
    stringStream << "Failed to set data for tag " << tag <<  std::endl;

    e.message = stringStream.str();
    throw (e);
  }
  
  return(0);
}

// void tags should only be used for param_template.data_t = user_T
// library user is responsible for writing a fetch routine
int InputParams::getParamData(std::string tag, void* &param_data)
{
  param_data = 0;
  
  return(0);
}

//
void InputParams::CheckSet(const char* tag_id, std::string &variable, std::string default_value)
{
  if(isParamSet(tag_id))
    getParamData(tag_id, variable);
  else
    variable = default_value;
}

void InputParams::CheckSet(const char* tag_id, int &variable, int default_value)
{
  if(isParamSet(tag_id))
    getParamData(tag_id, variable);
  else
    variable = default_value;
}

void InputParams::CheckSet(const char* tag_id, float &variable, float default_value)
{
  if(isParamSet(tag_id))
    getParamData(tag_id, variable);
  else
    variable = default_value;
}

void InputParams::CheckSet(const char* tag_id, bool &variable, bool default_value)
{
  if(isParamSet(tag_id))
    getParamData(tag_id, variable);
  else
    variable = default_value;
}

std::string InputParams::to_regex_expression(const std::string& path)
{
  //std::string expression("^" + path);
  std::string expression(path);

  replace_all(expression, '.', "\\.") ;
  replace_all(expression, '*', ".*") ;
  replace_all(expression, '?', "(.{1,1})") ;
  
  return expression ;
}

void InputParams::replace_all( std::string& source, char replace, const std::string& replacement)
{
  std::size_t pos = 0 ;
  while (pos < source.length() &&  std::string::npos != (std::size_t)(pos = source.find(replace, pos)) )
    {
      source.replace(pos, 1, replacement) ;
      pos += replacement.length() ;
    }
}
