/**
 * \file FileIO.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for the FileIO Class.
 *
 */
#ifndef _FILEIO_HH
#define _FILEIO_HH

#include "Exception.hh"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#define GZIP_MAGIC_byte1 0x1f
#define GZIP_MAGIC_byte2 0x8b
#define JUST64 64

namespace catlib {
  
  /// FileIO class is simply a collection of methods to simplify file
  /// IO tasks. It reads/writes text and binary data, compressed and
  /// uncompressed
  ///
  /// Caution: The caller is expected to free buffers allocated in the
  /// object by ''read'' methods.
  class FileIO {
    
  public:
    // N.B.: Reads will allocate buffer

    /// Read text file contents into buffer
    ///\param file_name   Name of the file to read
    ///\param file_buffer Address of pointer to file buffer
    ///\return            Number of bytes read (not including padding), will throw exception. 
    ///
    /// read pads end of buffer with newline and '\0'
    /// caller must free memory allocated for file_buffer
    ///
    size_t read_txt_file(std::string &file_name, char   ** file_buffer);

    /// Write text buffer to file
    ///\param file_name   Name of the file to write (or overwrite!)
    ///\param file_buffer Pointer to file buffer
    ///\param compress    If true, compress file (gzip)
    ///\return            0 on success, will throw exception.
    ///
    int    write_txt_file(std::string &file_name, char * file_buffer, bool compress = false);

    /// convert txt buffer (length of num_bytes) to vector of lines
    ///\param file_buffer  buffer of characters
    ///\param num_bytes    length of buffer
    ///\param line_vector  vector of buffer deliminated by newline character
    ///
    void   buffer_to_lines(char * file_buffer, size_t num_bytes, std::vector<std::string> &line_vector);
    
    /// Read binary file contents into buffer
    ///\param file_name   Name of the file to read
    ///\param file_buffer Address of pointer to file buffer
    ///\return            Number of bytes read from file
    ///
    /// caller must free memory allocated for file_buffer
    ///
    template<typename Data_t> inline
    size_t read_file(std::string &file_name, Data_t ** file_buffer);
    
    /// Read binary file contents into an existing buffer
    ///\param file_name   Name of the file to read
    ///\param file_buffer Address of pointer to file buffer
    ///\param buffer_size Size of file_buffer (bytes)
    ///\return            Number of bytes read from file, will throw exception
    ///
    template<typename Data_t> inline
    size_t read_file(std::string &file_name, Data_t * file_buffer, long buffer_size);

    /// Write binary buffer to file
    ///\param file_name   Name of the file to write (or overwrite!)
    ///\param data_buffer Pointer to data buffer
    ///\param data_size   Number bytes in data_buffer
    ///\param compress    If true, compress file (gzip)
    ///\return            0 on success, will throw exception.
    ///
    template <typename Data_t> inline
    int write_file(std::string &file_name, Data_t * data_buffer, size_t data_size, bool compress = false);

    /// make a directory - wrapper for ''man 2 mkdir''
    ///\param dir_name   Name of the directory to create
    ///\param mode       Mode of the new directory (mode_t defined in sys/stat.h)
    /// may throw exception
    ///
    /// Emulates ''mkdir -p'' creating any necessary additional directories
    //
    void make_dir(std::string dir_name, mode_t mode);

  private:
    
    size_t open_file(std::string file_name, FILE ** input, const char * mode);
    
    // lib provides buffer, return in bytes
    template <typename Data_t> inline
    size_t read_raw_data(std::string &file_name, FILE* file_ptr, size_t file_size, Data_t ** file_buffer);

    // user provides buffer, buffer_size in bytes
    template <typename Data_t> inline
    size_t read_raw_data(std::string &file_name, FILE* file_ptr, size_t file_size, Data_t * file_buffer, long buffer_size);

    
    // lib provides buffer, return in bytes
    template <typename Data_t> inline
    size_t read_compressed_data(std::string &file_name, size_t file_size, Data_t ** file_buffer);    

    // user provides buffer, buffer_size in bytes
    template <typename Data_t>
    inline
    size_t read_compressed_data(std::string &file_name, size_t file_size, Data_t * file_buffer, long buffer_size);

      
    // data size is in BYTES!!
    template <typename Data_t> inline
    int write_raw_data(std::string &file_name, Data_t * data_buffer, size_t data_size);
    
    // data_size in BYTES!!
    template <typename Data_t> inline
    int write_compressed_data(std::string &file_name, Data_t * data_buffer, size_t data_size);
    
  };

  //
  // Function definitions
  //

  // templated functions are not 'written' until the template is
  // defined (e.g., Data_t ==> float) so the definitions here are
  // required to be included into modules that then define the
  // template to be a type. The 'actual' function is created at
  // compile time for the module.
  
  template<typename Data_t>
  inline
  size_t FileIO::read_file(std::string &file_name, Data_t ** file_buffer)
  {
    FILE * file_ptr  = 0;
    size_t file_size = 0;
    
    try {
      file_size = open_file(file_name, &file_ptr, "r");
    }
    catch (Exception_t e) {
      throw(e);
    }
    
    // check for compression
    char test_buf[JUST64];
    
    int test_bytes = fread(test_buf, sizeof(char), JUST64, file_ptr);
    
    // rewind file
    fseek(file_ptr, 0, SEEK_SET);
    
    if(! ((test_bytes == JUST64) || (test_bytes == file_size)) ){
      // exception
      std::ostringstream stringStream;
      
      stringStream << "failed to read '" << file_name << "'" << std::endl;
      stringStream << strerror(errno) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      fclose(file_ptr);
      
      throw (e);
    }
    
    unsigned char byte1 = test_buf[0];
    unsigned char byte2 = test_buf[1];
    
    try {
      if((byte1 == GZIP_MAGIC_byte1) && (byte2 == GZIP_MAGIC_byte2)){
	// likely a gzip file
	fclose(file_ptr);
	file_size = read_compressed_data(file_name, file_size, file_buffer);
      }
      else {
	// normal file
	file_size = read_raw_data(file_name, file_ptr, file_size, file_buffer);
      }
    }
    catch(Exception_t e) {
      throw (e);
    }
    
    return(file_size);
  };

  //
  //
  template <typename Data_t>
  inline
  int FileIO::write_file(std::string &file_name, Data_t * data_buffer, size_t data_size, bool compress) 
  {
    try{
      if(compress)
	write_compressed_data(file_name, data_buffer, data_size);
      else
	write_raw_data(file_name, data_buffer, data_size);
    }
    catch (Exception_t e) {
      throw(e);
    }
    
    return(0);
  };

  //
  //
  template <typename Data_t>
  inline
  size_t FileIO::read_raw_data(std::string &file_name, FILE* file_ptr, size_t file_size, Data_t ** file_buffer)
  {
    // normalized file_size to member size
    if(0 != file_size % sizeof(Data_t)){
      std::ostringstream stringStream;
      stringStream << "File '" << file_name << "' does not contain a multiple number of " << sizeof(Data_t) << " bytes." << std::endl;
      stringStream << "Failed to read " << file_size << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      fclose(file_ptr);    
      throw (e);
    }
    
    size_t data_count = file_size / sizeof(Data_t);
    
    *file_buffer = new Data_t[data_count];
    
    if(!*file_buffer){
      // exception
      std::ostringstream stringStream;
      stringStream << "Memory exhausted reading file '" << file_name << "'." << std::endl;
      stringStream << "Failed to allocate " << file_size << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      fclose(file_ptr);
      throw (e);
    }
    
    // zero buffer
    memset((unsigned char *)*file_buffer, 0, file_size);
    
    errno = 0;
    
    size_t n;
    size_t red = 0;
    
    while(red < data_count){
      
      // reading in Data_t increments
      n = fread(&(*file_buffer)[red], sizeof(Data_t), (data_count - red), file_ptr);
      
      if(n == 0)
	break;
      
      if(n < 1){
	// exception
	std::ostringstream stringStream;
	stringStream << "Bad read in file '" << file_name <<"'." <<  std::endl;
	stringStream << strerror(errno) << std::endl;
	
	Exception_t e;
	e.message = stringStream.str();
	
	fclose(file_ptr);
	throw (e);
      }
      
      red += n;
    }
    
    if(red != data_count){
      // short read
      // exception
      std::ostringstream stringStream;
      stringStream << "Bad read in file '" << file_name <<"'." <<  std::endl;
      stringStream << "Expecting " << data_count;
      stringStream << " but read " << red << std::endl;
      stringStream << strerror(errno) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      fclose(file_ptr);
      throw (e);
    }
    
    fclose(file_ptr);
    
    return(file_size);
  };
  
  template <typename Data_t>
  inline
  size_t FileIO::read_compressed_data(std::string &file_name, size_t file_size, Data_t ** file_buffer)
  {
    // reads compressed or uncompressed files
    gzFile the_fh  = gzopen(file_name.c_str(), "r");
    
    if(the_fh == 0){
      // error
      std::ostringstream stringStream;
      stringStream << "failed to open '" << file_name << "'" << std::endl;
      stringStream << strerror(errno) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
    
    long   PAGESIZE = sysconf(_SC_PAGESIZE);
    long   LARGESIZE = PAGESIZE * 1024 * 10; // ~40 mb
    
    unsigned char *uncomp_buffer = new unsigned char[LARGESIZE];
    
    if(!uncomp_buffer){
      std::ostringstream stringStream;
      stringStream << "Memory exhausted reading file '" << file_name << "'." << std::endl;
      stringStream << "Failed to allocate " << LARGESIZE << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      gzclose(the_fh);
      throw (e);
    }
    
    // note file_size is for compressed file!
    // guess compression is 3:1
    // receiving_vector will automatically resize if necessary
    size_t size_guess = file_size * 3;
    
    std::vector<unsigned char> receiving_vector;
    receiving_vector.reserve(size_guess);
    
    // init buffer
    gzbuffer(the_fh, LARGESIZE);
    
    int bytes_red  = 0;
    
    do {
      
      // reading in byte increments
      bytes_red = gzread(the_fh, uncomp_buffer, LARGESIZE);
      
      if(bytes_red < 0){
	// error
	std::ostringstream stringStream;
	stringStream << "failed to decompress '" << file_name << "'" << std::endl;
	stringStream << gzerror(the_fh, NULL) << std::endl;
	
	Exception_t e;
	e.message = stringStream.str();
	delete[] uncomp_buffer;
	gzclose(the_fh);
	throw (e);
      }
      
      if(bytes_red != 0){
	unsigned char * end = uncomp_buffer + bytes_red;
	receiving_vector.insert(receiving_vector.end(), uncomp_buffer, end);
      }
      
    } while (bytes_red > 0);
    
    // close
    gzclose(the_fh);
    delete[] uncomp_buffer;
    
    //
    // read the data into a buffer
    //
    // normalized file_size to member size
    if(0 != receiving_vector.size() % sizeof(Data_t)){
      std::ostringstream stringStream;
      stringStream << "File '" << file_name << "' does not contain a multiple number of " << sizeof(Data_t) << " bytes." << std::endl;
      stringStream << "Failed to read " << receiving_vector.size() << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
    
    size_t data_count = receiving_vector.size() / sizeof(Data_t);
    
    *file_buffer = new Data_t[data_count];
    
    if(!*file_buffer){
      // exception
      std::ostringstream stringStream;
      stringStream << "Memory exhausted reading file '" << file_name << "'." << std::endl;
      stringStream << "Failed to allocate " << file_size << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
    
    // zero buffer
    memset((unsigned char *)*file_buffer, 0, receiving_vector.size());
    
    memcpy((unsigned char *)*file_buffer, receiving_vector.data(), receiving_vector.size());
    
    // bytes
    return(receiving_vector.size());
  };

  //
  //
  template <typename Data_t>
  inline
  int FileIO::write_raw_data(std::string &file_name, Data_t * data_buffer, size_t data_size)
  {
    FILE * file_ptr  = 0;
    
    try {
      open_file(file_name, &file_ptr, "w+");
    }
    catch (Exception_t e) {
      throw(e);
    }
    
    errno = 0;
    size_t n = 0;
    size_t rit = 0;
    
    while(rit < data_size){
      
      n = fwrite((unsigned char *)&(data_buffer)[rit], sizeof(unsigned char), (data_size - rit), file_ptr);
      
      if(n < 1){
	// exception
	std::ostringstream stringStream;
	stringStream << "Bad write in file '" << file_name <<"'." <<  std::endl;
	stringStream << strerror(errno) << std::endl;
	
	Exception_t e;
	e.message = stringStream.str();
	
	fclose(file_ptr);
	throw (e);
      }
      
      if(n == 0)
	break;
      
      rit += n;
    }
    
    if(rit != data_size){
      // exception
      std::ostringstream stringStream;
      stringStream << "Bad write in file '" << file_name <<"'." <<  std::endl;
      stringStream << "Expecting " << data_size;
      stringStream << " but wrote " << rit << std::endl;
      
      stringStream << strerror(errno) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      fclose(file_ptr);
      throw (e);
    }
    
    fclose(file_ptr);
    
    return(0);
  };
  
  //
  //
  template <typename Data_t>
  inline
  int FileIO::write_compressed_data(std::string &file_name, Data_t * data_buffer, size_t data_size)
  {
    // does file name end in .gz or .gzip
    // if not end with '.gz'
    bool name_is_ok = false;
    std::string ending = ".gzip";
    if(file_name.length() >= ending.length()){
      if(0 == file_name.compare (file_name.length() - ending.length(), ending.length(), ending))
	// ends with .gzip
	name_is_ok = true;
    }
    if(!name_is_ok){
      ending = ".gz";
      if(file_name.length() >= ending.length()){
	if(0 == file_name.compare (file_name.length() - ending.length(), ending.length(), ending))
	  // ends with .gz
	  name_is_ok = true;
      }
    }
    
    if(!name_is_ok)
      // ends with .gz!!
      file_name += ending;
    
    gzFile file_ptr = gzopen(file_name.c_str(), "w6");
    
    if(!file_ptr){
      // exception
      std::ostringstream stringStream;
      stringStream << "File open failed for file " << file_name << std::endl;
      stringStream << gzerror(file_ptr, NULL) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
    
    size_t PAGESIZE  = sysconf(_SC_PAGESIZE);
    size_t LARGESIZE = PAGESIZE * 1024 * 25; // ~100 mb
    
    // init buffer
    gzbuffer(file_ptr, LARGESIZE);
    
    int bytes_red  = 0;
    int bytes_writ;
    int bytes_left = data_size;
    
    do {
      
      bytes_writ = gzwrite(file_ptr, (unsigned char *)&data_buffer[bytes_red], bytes_left);
      
      if(bytes_writ < 0){
	std::ostringstream stringStream;
	stringStream << "File write failed for file '" << file_name << "'." <<  std::endl;
	stringStream << gzerror(file_ptr, NULL) << std::endl;
	
	Exception_t e;
	e.message = stringStream.str();
	
	gzclose(file_ptr);
	throw (e);
      }
      
      if(bytes_writ == 0)
	break;
      
      bytes_red += bytes_writ;
      
      bytes_left = data_size - bytes_red;
      
    } while( bytes_left > 0 );
    
    if(bytes_writ != data_size){
      // exception
      std::ostringstream stringStream;
      stringStream << "Bad write in file '" << file_name <<"'." <<  std::endl;
      stringStream << gzerror(file_ptr, NULL) << std::endl;
      stringStream << "Expecting " << data_size;
      stringStream << " but wrote " << bytes_writ << std::endl;
      stringStream << gzerror(file_ptr, NULL) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      gzclose(file_ptr);
      throw (e);
    }
    
    // close
    gzclose(file_ptr);
    
    return(0);
  };


  // caller supplies buffer of buffer_size (bytes)
  template<typename Data_t> inline
  size_t FileIO::read_file(std::string &file_name, Data_t * file_buffer, long buffer_size)
  {
    FILE * file_ptr  = 0;
    size_t file_size = 0;
    
    try {
      file_size = open_file(file_name, &file_ptr, "r");
    }
    catch (Exception_t e) {
      throw(e);
    }
    
    // check for compression
    char test_buf[JUST64];
    
    int test_bytes = fread(test_buf, sizeof(char), JUST64, file_ptr);
    
    // rewind file
    fseek(file_ptr, 0, SEEK_SET);
    
    if(! ((test_bytes == JUST64) || (test_bytes == file_size)) ){
      // exception
      std::ostringstream stringStream;
      
      stringStream << "failed to read '" << file_name << "'" << std::endl;
      stringStream << strerror(errno) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      fclose(file_ptr);
      
      throw (e);
    }
    
    unsigned char byte1 = test_buf[0];
    unsigned char byte2 = test_buf[1];
    
    try {
      if((byte1 == GZIP_MAGIC_byte1) && (byte2 == GZIP_MAGIC_byte2)){
	// likely a gzip file
	fclose(file_ptr);
	file_size = read_compressed_data(file_name, file_size, file_buffer, buffer_size);
      }
      else {
	// normal file
	file_size = read_raw_data(file_name, file_ptr, file_size, file_buffer, buffer_size);
      }
    }
    catch(Exception_t e) {
      throw (e);
    }
    
    return(file_size);
  }
  
  //
  //
  template <typename Data_t>
  inline
  size_t FileIO::read_raw_data(std::string &file_name, FILE* file_ptr, size_t file_size, Data_t * file_buffer, long buffer_size)
  {
    // normalized buffer_size to member size
    if(0 != buffer_size % sizeof(Data_t)){
      std::ostringstream stringStream;
      stringStream << "CODING ERROR: " << std::endl;
      stringStream << "Failed to read " << file_size << " bytes." << std::endl;
      stringStream << "The buffer_size requested " << buffer_size << " is not contain a multiple number of " << sizeof(Data_t) << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }

    size_t data_count = buffer_size / sizeof(Data_t);

    // zero buffer
    memset((unsigned char *)file_buffer, 0, buffer_size);
    
    errno = 0;
    
    size_t n;
    size_t red = 0;
    
    while(red < data_count){
      
      // reading in Data_t increments
      n = fread(&(file_buffer)[red], sizeof(Data_t), (data_count - red), file_ptr);
      
      if(n == 0)
	break;
      
      if(n < 1){
	// exception
	std::ostringstream stringStream;
	stringStream << "Bad read in file '" << file_name <<"'." <<  std::endl;
	stringStream << strerror(errno) << std::endl;
	
	Exception_t e;
	e.message = stringStream.str();
	
	fclose(file_ptr);
	throw (e);
      }
      
      red += n;
    }
    
    
    fclose(file_ptr);
    
    return(red * sizeof(Data_t));
  };

  template <typename Data_t>
  inline
  size_t FileIO::read_compressed_data(std::string &file_name, size_t file_size, Data_t * file_buffer, long buffer_size)
  {
    // normalized buffer_size to member size
    if(0 != buffer_size % sizeof(Data_t)){
      std::ostringstream stringStream;
      stringStream << "CODING ERROR: " << std::endl;
      stringStream << "Failed to read " << file_size << " bytes." << std::endl;
      stringStream << "The buffer_size requested " << buffer_size << " is not contain a multiple number of " << sizeof(Data_t) << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
    
    // reads compressed or uncompressed files
    gzFile the_fh  = gzopen(file_name.c_str(), "r");
    
    if(the_fh == 0){
      // error
      std::ostringstream stringStream;
      stringStream << "failed to open '" << file_name << "'" << std::endl;
      stringStream << strerror(errno) << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
    
    long   PAGESIZE = sysconf(_SC_PAGESIZE);
    long   LARGESIZE = PAGESIZE * 1024 * 10; // ~40 mb
    
    unsigned char *uncomp_buffer = new unsigned char[LARGESIZE];
    
    if(!uncomp_buffer){
      std::ostringstream stringStream;
      stringStream << "Memory exhausted reading file '" << file_name << "'." << std::endl;
      stringStream << "Failed to allocate " << LARGESIZE << " bytes." << std::endl;
      
      Exception_t e;
      e.message = stringStream.str();
      
      gzclose(the_fh);
      throw (e);
    }
    
    // vector to hold buffer we read
    std::vector<unsigned char> receiving_vector;
    
    // init buffer
    gzbuffer(the_fh, LARGESIZE);
    
    int  bytes_red  = 0;
    long total_bytes_red  = 0;
    
    do {
      
      // reading in byte increments
      bytes_red = gzread(the_fh, uncomp_buffer, LARGESIZE);
      
      if(bytes_red < 0){
	// error
	std::ostringstream stringStream;
	stringStream << "failed to decompress '" << file_name << "'" << std::endl;
	stringStream << gzerror(the_fh, NULL) << std::endl;
	
	Exception_t e;
	e.message = stringStream.str();
	delete[] uncomp_buffer;
	gzclose(the_fh);
	throw (e);
      }
      
      if(bytes_red != 0){
	unsigned char * end = uncomp_buffer + bytes_red;
	receiving_vector.insert(receiving_vector.end(), uncomp_buffer, end);
      }

      total_bytes_red += bytes_red;
      
    } while ((bytes_red > 0) && (total_bytes_red < buffer_size));
    
    // close
    gzclose(the_fh);
    delete[] uncomp_buffer;
    
    //
    // read the data into a buffer
    //
    long available_bytes = 0;
    if(total_bytes_red < buffer_size)
      available_bytes = total_bytes_red;
    else
      available_bytes = buffer_size;
    
    // zero buffer
    memset(file_buffer, 0, buffer_size);
    
    memcpy((unsigned char *)file_buffer, receiving_vector.data(), available_bytes);
    
    // bytes
    return(available_bytes);
  };


};

#endif
