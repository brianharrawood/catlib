/**
 * \file Exception.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for CTkit Exception struct.
 *
 */
#ifndef __EXCEPTION_HH
#define __EXCEPTION_HH

#include <string>
#include <sstream>

namespace catlib {

  /// \struct Exception
  ///
  /// The catlib::Exception is a simple exception passing structure
  ///\code
  /// std::string message;
  ///\endcode
  ///
  struct Exception{
    std::string message;
  };
  typedef struct Exception Exception_t;

  inline void AllocationException(const char * file, int line, size_t size){
    std::ostringstream stringStream;
    
    stringStream << "failed to allocate " << size << " bytes." << std::endl;
    
    stringStream << std::string(file) << ": " << " Line:" << line << std::endl;
    
    catlib::Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }

};

#endif
