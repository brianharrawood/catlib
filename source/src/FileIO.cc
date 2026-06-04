#include "FileIO.hh"

using namespace catlib;

//
// reading
//

// allocates text file buffer
// buffer 2 bytes more than files size
// caller must free!
size_t FileIO::read_txt_file(std::string &file_name, char ** file_buffer)
{
  FILE * file_ptr  = 0;
  size_t file_size = 0;
  size_t n;
  
  try {
    file_size = open_file(file_name, &file_ptr, "r");
  }
  catch (Exception_t e) {
    throw(e);
  }

  // for char buffers - add room for new-line and NULL terminator.
  size_t file_buffer_len = file_size+2;
  
  *file_buffer = new char[file_buffer_len];
  
  if(!*file_buffer){
    // exception
    AllocationException(__FILE__, __LINE__, file_buffer_len);
  }
  
  // zero buffer
  memset(*file_buffer, 0, file_buffer_len);
  
  errno = 0;

  size_t red = 0;
  
  while(red < file_size){
    
    n = fread(&(*file_buffer)[red], sizeof(char), (file_size - red), file_ptr);
    
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
  
  // added for parsing the last line!
  (*file_buffer)[file_size] = '\n';
  
  fclose(file_ptr);
  
  return(file_buffer_len);
}

//
// writing
//

// included for symmetry  just a pass through
int FileIO::write_txt_file(std::string &file_name, char * file_buffer, bool compress)
{
  size_t data_size = strlen(file_buffer);
  
  try{
    if(compress)
      write_compressed_data(file_name, file_buffer, data_size);
    else
      write_raw_data(file_name, file_buffer, data_size);
  }
  catch (Exception_t e) {
    throw(e);
  }
  
  return(0);
}


// opens file with error handling
// caller must close
size_t FileIO::open_file(std::string file_name, FILE ** input, const char * mode)
{
  size_t file_size = 0;

  // stat file to check for directories, sockets, etc...
  struct stat file_stat;
  int rc = stat(file_name.c_str(), &file_stat);
  
  if(mode[0] == 'r' && rc == -1){
    // reading existing file
    // exception
    std::ostringstream stringStream;
    stringStream << "Could not stat file. '" << file_name << "'" << std::endl;
    stringStream << strerror(errno) << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    throw (e);
  }
  
  if(mode[0] == 'r' && !S_ISREG(file_stat.st_mode)) {
    // exception
    std::ostringstream stringStream;
    stringStream << "File '" << file_name << "' must be 'regular' file." << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    throw (e);
  }
  
  // open
  *input = fopen(file_name.c_str(), mode);
  
  if(!*input){
    // exception
    std::ostringstream stringStream;
    stringStream << "File open failed for file '" << file_name << "'." <<  std::endl;
    stringStream << strerror(errno) << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    throw (e);
  }
  
  fseek(*input, 0, SEEK_END);
  file_size = ftell(*input);
  fseek(*input, 0, SEEK_SET);

  return(file_size);
}

void FileIO::buffer_to_lines(char * file_buffer, size_t num_bytes, std::vector<std::string> &line_vector)
{
  char * start  = file_buffer;
  char * end    = file_buffer;
  size_t i = 0;
  
  while((end != NULL) && (i < num_bytes)){

    // linux line delimiter
    if(*end == '\n'){
      // plus 1 preserves the \n
      line_vector.push_back(std::string(start, (end-start)+1));
      start = end + 1;
    }
    
    end++;
    i++;
  }
}

// duplicated in Log.hh!!
void FileIO::make_dir(std::string dir_name, mode_t mode)
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
