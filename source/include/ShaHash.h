/**
 * \file ShaHash.h
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file defining crypto hashing functions.
 *
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

#include <iostream>

extern "C" {
  int  hash_string(char *string, char outputBuffer[65]);
  int  hash_file(char *path, char hash[65]);
  void sha256_hash_to_string (unsigned char hash[SHA256_DIGEST_LENGTH], char outputBuffer[65]);
};

