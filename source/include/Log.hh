/**
 * \file Log.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for simple Logging feature.
 *
 * The Log class is functional but has been implemented into any of
 * the CTkit applications.
 */

#ifndef __LOG_HH_
#define __LOG_HH_

#include <string>
#include <vector>
#include <sstream>

#include "SyncLock.hh"

namespace catlib {
  
  // levels
  enum LogLevel_t {
    LOG_ERROR,  // no remote procedure
    LOG_INFO,
    LOG_DEBUG
  };

  class Log {
  public:
    Log();
    
    ~Log();
    
    void Init(std::string log_dir, std::string log_name, LogLevel_t thelevel);
    
    void LogInfo(LogLevel_t, std::string Msg);
    
    void Write2Log(std::string text);
    
    void setLevel(enum LogLevel_t level);
    
  private:
    
    void make_dir(std::string dir_name, mode_t mode);
    void open_log();
    
    std::string path_;
    FILE      * file_handle_;
    int         level_;
    
    SyncLock    the_Lock_;         // prevent message interleaving
    
  };
  
};
#endif
