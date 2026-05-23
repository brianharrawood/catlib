#include "Log.hh"

#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include <ctime>

#include "Exception.hh"

using namespace catlib;

Log::Log() {;}

Log::~Log()
{
  std::string msg = "Logging completed\n";
  LogInfo(LOG_INFO, msg);

  fflush(file_handle_);
  fclose(file_handle_);
}

void Log::Init(std::string log_dir, std::string log_name, LogLevel_t the_level)
{
  level_ = the_level;
  
  try {
    make_dir(log_dir, 0755);
    path_ = log_dir + std::string("/") + log_name;
    open_log();
  }
  catch (Exception_t &e){
    // exception
    std::ostringstream stringStream;
    stringStream << "ERROR - Failed open log\n";
    stringStream << e.message;
    
    Exception_t e_new;
    e_new.message = stringStream.str();
    
    throw (e_new);
  }

  std::string log_level_str = "None";
  
  switch (level_) {
  case LOG_ERROR:
    log_level_str = "LOG_ERROR";
    break;
  case LOG_INFO:
    log_level_str = "LOG_INFO";
    break;
  case LOG_DEBUG:
    log_level_str = "LOG_DEBUG";
    break;
  }

  std::string msg = "Logging started\nLog Level set to: " + log_level_str;
  LogInfo(LOG_INFO, msg);

}

void Log::LogInfo(LogLevel_t level, std::string message)
{
 if(level > level_)
    return;

  the_Lock_.Lock();
  
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);

  char timeString[32];  // space for "HH:MM:SS:"
  strftime(timeString, sizeof(timeString), "*** %H:%M:%S:\n", &tm);

  errno = 0;
  
  fwrite(timeString, sizeof(char), strlen(timeString), file_handle_);  
  if(errno){
    std::ostringstream stringStream;
    stringStream << strerror(errno) << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    the_Lock_.Unlock();
    throw (e);
  }

  // separate log messages with newlines
  message += "\n\n";
  
  fwrite(message.c_str(), sizeof(char), message.length(), file_handle_);
  if(errno){
    std::ostringstream stringStream;
    stringStream << strerror(errno) << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    the_Lock_.Unlock();
    throw (e);
  }

  fflush(file_handle_);

  the_Lock_.Unlock();
}

void Log::Write2Log(std::string text)
{
  // dump text without time stamp into log
  fwrite(text.c_str(), sizeof(char), text.length(), file_handle_);
  if(errno){
    std::ostringstream stringStream;
    stringStream << strerror(errno) << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    the_Lock_.Unlock();
    throw (e);
  }
  
  fflush(file_handle_);

}

void Log::open_log()
{
  // open for reading
  file_handle_ = fopen(path_.c_str(), "w+");
  
  if(!file_handle_){
    // exception
    std::ostringstream stringStream;
    stringStream << "File open failed for '" << path_ << "'" <<  std::endl;
    stringStream << strerror(errno) << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }

  // in case file exists (w+ overrides)
  errno = 0;
  
  return;
}


void Log::make_dir(std::string dir_name, mode_t mode)
{

  // parse dir_name to simulate 'mkdir -p'
  std::vector <std::string> tokens;
  
  // stringstream class check1
  std::stringstream parseStream(dir_name);
  std::string token;
  
  // Tokenizing w.r.t. space '/'
  while(getline(parseStream, token, '/'))
    tokens.push_back(token);

  std::string name_accumulator = "";
  
  // Printing the token vector
  for(int i = 0; i < tokens.size(); i++){
    
    // build path a token at a time
    name_accumulator += tokens[i];
    name_accumulator += "/";

    errno = 0;
    const char * d_name = name_accumulator.c_str();
    
    int rc = mkdir(d_name, mode);
    
    if(rc != 0) {
      // error
      // allow error for pre-existing directories
      if(errno == EEXIST){
	// but, could be a file
	struct stat s;
	if( stat(d_name,&s) == 0 ){
	  if( s.st_mode & S_IFDIR )
	    {
	      // it is an existing directory
	      continue;
	    }
	  return;
	}
      }
      // else, exception
      std::ostringstream stringStream;
      stringStream << "ERROR - Failed to make directory:" << dir_name <<  std::endl;
      stringStream << strerror(errno);
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
  }
  
  // else
  return;
}

