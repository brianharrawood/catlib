#include "ShaHash.h"
// updated to ssl 3.0 02/2024

int hash_file(char *path, char outputBuffer[65])
{
  FILE *in_file = fopen(path, "rb");
  
  if(!in_file){
    std::cerr << "failed to open '" << path << "'" << std::endl;
    std::cerr << strerror(errno) << std::endl;
    return (-1);
  }
  
  // for file read
  const size_t BUF_SIZE = 32 * 1024;
  unsigned char* in_buf = new unsigned char[BUF_SIZE];
  
  if(!in_buf){
    std::cerr << "Memory exhausted reading file '" << path << "'." << std::endl;
    std::cerr << " At " << __FILE__ << ":" <<__LINE__ << std::endl; 
    std::cerr << "Failed to allocate " << BUF_SIZE << " bytes." << std::endl;
    fclose(in_file);
    return(-1);
  }

  // digest context
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  int ok = EVP_DigestInit(ctx, EVP_sha3_256());
  
  if(!ok){
    std::cerr << "Error in EVP_DigestInit() at " << __FILE__ << ":" <<__LINE__ << std::endl; 
    return(-1);
  }

  // read file and update digest
  size_t bytes_read = 0;
  while((bytes_read = fread(in_buf, 1, BUF_SIZE, in_file))){
    ok = EVP_DigestUpdate(ctx, in_buf, bytes_read);
    if(!ok){
      std::cerr << "Error in EVP_DigestUpdate() at " << __FILE__ << ":" <<__LINE__ << std::endl; 
      return(-1);
    }
  }
  
  // put digest into hash buffer
  unsigned char hash[SHA256_DIGEST_LENGTH];
  ok = EVP_DigestFinal(ctx, hash, NULL);
  if(!ok){
    std::cerr << "Error in EVP_DigestFinal() at " << __FILE__ << ":" <<__LINE__ << std::endl; 
    return(-1);
  } 
  
  EVP_MD_CTX_free(ctx);
  delete[] in_buf;
  
  // put hash into printable chars
  sha256_hash_to_string(hash, outputBuffer);

  return (0);
}

int hash_string(char *string, char outputBuffer[65])
{
  // digest context
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  int ok = EVP_DigestInit(ctx, EVP_sha3_256());
  
  if(!ok){
    std::cerr << "Error in EVP_DigestInit() at " << __FILE__ << ":" <<__LINE__ << std::endl; 
    return(-1);
  }

  // read file and update digest
  ok = EVP_DigestUpdate(ctx, string, strlen(string));
  if(!ok){
    std::cerr << "Error in EVP_DigestUpdate() at " << __FILE__ << ":" <<__LINE__ << std::endl; 
    return(-1);
  }
  
  // put digest into hash buffer
  unsigned char hash[SHA256_DIGEST_LENGTH];
  ok = EVP_DigestFinal(ctx, hash, NULL);
  if(!ok){
    std::cerr << "Error in EVP_DigestFinal() at " << __FILE__ << ":" <<__LINE__ << std::endl; 
    return(-1);
  } 
  
  EVP_MD_CTX_free(ctx);
  
  // put hash into printable chars
  sha256_hash_to_string(hash, outputBuffer);

  return (0);
}

void sha256_hash_to_string (unsigned char hash[SHA256_DIGEST_LENGTH], char outputBuffer[65])
{
  
  for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
      sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
  
  outputBuffer[64] = 0;
}


#if 0
int sha256_string(char *string, char outputBuffer[65])
{
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, string, strlen(string));
  SHA256_Final(hash, &sha256);

  for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
      sprintf(outputBuffer + (i * 2), "%02x", hash[i]);

  outputBuffer[64] = 0;

  return(0);
}
#endif


#if 0
int sha256_file(char *path, char outputBuffer[65])
{
  FILE *file = fopen(path, "rb");

  if(!file)
    return (-1);
  
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_CTX sha256;

  SHA256_Init(&sha256);

  int bytesRead = 0;
  int bufSize = 32768;

  unsigned char *buffer = new unsigned char(bufSize);
  if(!buffer)
    return (-1);
  
  while((bytesRead = fread(buffer, 1, bufSize, file)))
      SHA256_Update(&sha256, buffer, bytesRead);

  SHA256_Final(hash, &sha256);
  
  sha256_hash_to_string(hash, outputBuffer);

  fclose(file);

  delete[] buffer;

  return(0);
}
#endif
