// exercise FileIO class
#include "FileIO.hh"
#include "ShaHash.h"
#include "Exception.hh"
#include <string>

using namespace catlib;

int main()
{
  int error_count = 0;
  
  FileIO filer;
  size_t n;
  int rc;
  
  std::string input_txt_file = "./input/test.txt";
  std::string output_txt_file = "./output/test.txt";
  
  std::string input_flt_file = "./input/test.flt";
  std::string output_flt_file = "./output/test.flt";
  
  char  * txt_buffer;
  float * flt_buffer;
  
  try {
    n = filer.read_txt_file(input_txt_file, &txt_buffer);
  }
  catch (Exception_t e) {
    std::cerr << e.message;
    error_count++;
  }
  
  std::vector<std::string> lines;
  lines.reserve(n/32); // wild guess
  
  filer.buffer_to_lines(txt_buffer, n, lines);
  
  if(lines.size() != 17113){
    std::cerr << "Error in buffer_to_lines!" << std::endl;
    error_count++;
  }
  
  try {
    rc = filer.write_txt_file(output_txt_file, txt_buffer);
  }
  catch (Exception_t e) {
    std::cerr << e.message;
    error_count++;
  }
  
  
  try {
    n = filer.read_file(input_flt_file, &flt_buffer);
  }
  catch (Exception_t e) {
    std::cerr << e.message;
    error_count++;
  }
  
  try {
    rc = filer.write_file(output_flt_file, flt_buffer, n, false);
  }
  catch (Exception_t e) {
    std::cerr << e.message;
    error_count++;
  }
  
  // write compressed
  try {
    rc = filer.write_file(output_flt_file, flt_buffer, n, true);
  }
  catch (Exception_t e) {
    std::cerr << e.message;
    error_count++;
  }
  
  // read compressed
  delete[] flt_buffer;
  flt_buffer = 0;
  std::string input_flt_file_gz = "./input/test.flt.gz";
  try {
    n = filer.read_file(input_flt_file_gz, &flt_buffer);
  }
  catch (Exception_t e) {
    std::cerr << e.message;
    error_count++;
  }
  
  std::string output_flt_file_01 = "./output/test.flt.01";
  try {
    rc = filer.write_file(output_flt_file_01, flt_buffer, n, false);
  }
  catch (Exception_t e) {
    std::cerr << e.message;
    error_count++;
  }
  
  // hash a file
  char hash[65];
  hash_file((char *)output_flt_file.c_str(), hash);
  
  //std::cout << "Hash of " << output_flt_file << " is " << std::string(hash) << std::endl;
  
  char s[32] = "hash ole";
  
  hash_string((char *)s, hash);
  
  //std::cout << "Hash of '" << std::string(s) << "' is " << std::string(hash) << std::endl;
  
  // just get a buffer
  float  float_bufferA[64];
  float  float_bufferB[64];
  size_t red_flts;
  
  red_flts = filer.read_file(input_flt_file, float_bufferA, 64L);
  red_flts = filer.read_file(input_flt_file_gz, float_bufferB, 64L);
  
  rc = memcmp(float_bufferA, float_bufferB, 64);
  
  if(rc != 0){
    std::cerr << "Bufer compare failed!" << std::endl;
    error_count++;
  }
  
  if(error_count == 0)
    std::cerr << "Test passed!" << std::endl;
  else
    std::cerr << "Test Failed!" << std::endl;
    
  
  return(0);
}

