#include "XCAT_compress.h"

//
int main(int argc, char ** argv)
{
  std::cout << "This will take several minutes! Phantom is 7.3 GB! " << std::endl;
  
  XCATPhantomCompress compressor;
  std::string phantom_file = "./input/phantom_2150_2150_1600_.bin.gz";

  // need this for comparison testing
  unsigned long   raw_size = 0;
  unsigned char * raw_buffer = 0;
  FileIO file;
  try {
    std::cerr << "Reading Phantom: " << phantom_file << std::endl;
    raw_size = file.read_file(phantom_file, &raw_buffer);
  } catch (Exception_t e) {
    std::cerr << e.message << std::endl;
    exit(EXIT_FAILURE);
  }
  
  tNode_t       * default_root = 0;

  //
  // base test
  //
  try {
    std::cerr << "XCAT (rle) Compressing Phantom: " << phantom_file << std::endl;
    compressor.LoadFile(phantom_file);
  } catch (Exception_t e) {
    std::cerr << e.message << std::endl;
    exit(EXIT_FAILURE);
  }

#if 1
  {
    std::cerr << "Base test" << std::endl;
    // test
    std::cerr << "Comparing raw values with compressed values" << std::endl;
    compare_values(raw_buffer, raw_size, compressor, default_root);
    
    std::cerr << "Speed test..." << std::endl;
    speed_test(raw_buffer, raw_size, compressor, default_root);
  }
#endif
  
  // writes to local dir
  int rc = compressor.WriteCompressedFile();
  
  //
  // read rle file test we just wrote
  //
  std::cerr << "rle input test" << std::endl;
  
  std::cerr << std::endl;
  phantom_file = "phantom_2150_2150_1600_.bin.rle";
  
  try {
    std::cerr << "Phantom: " << phantom_file << std::endl;
    compressor.LoadFile(phantom_file);
  } catch (Exception_t e) {
    std::cerr << e.message << std::endl;
    exit(EXIT_FAILURE);
  }
  
#if 1
  {
    // test
    std::cerr << "Comparing raw values with compressed values" << std::endl;
    compare_values(raw_buffer, raw_size, compressor, default_root);
    
    std::cerr << "Speed test..." << std::endl;
    speed_test(raw_buffer, raw_size, compressor, default_root);
  }
#endif
  
  //
  // read relocated buffer test
  // 
  std::cerr << std::endl;
  unsigned long  buffer_size;
  unsigned char * compressed_buffer = compressor.getCompressedBuffer(&buffer_size);

  //  make room for the buffer at new location
  unsigned char * relocated_buf = new unsigned char[buffer_size];

  // copy the compressed buffer to new location
  memcpy(relocated_buf, compressed_buffer, buffer_size);

  // adjust the pointers at the new location
  tNode_t * relocated_root = compressor.RelocateCompressionBuffer(relocated_buf);
    
#if 1
  {
    std::cerr << "relocation test" << std::endl;
    // test
    std::cerr << "Comparing raw values with relocated compressed values..." << std::endl;
    compare_values(raw_buffer, raw_size, compressor, relocated_root);
    
    std::cerr << "Speed test..." << std::endl;
    speed_test(raw_buffer, raw_size, compressor, relocated_root);
  }
#endif
  
  delete [] raw_buffer;

  //
  // read phantom to raw
  //
  compressor.LoadFile(phantom_file, false);

  raw_buffer = compressor.getRawBuffer(&raw_size);

  {
    std::cerr << "relocation test" << std::endl;
    // test
    std::cerr << "Loaded Raw Buffer - Comparing raw values with relocated compressed values..." << std::endl;
    compare_values(raw_buffer, raw_size, compressor, relocated_root);
  }  
  
  unlink(phantom_file.c_str());

  std::cerr << "No errors." << std::endl;
  
  return(0);
}

//
// fairly exhaustive excercise
//
void compare_values(unsigned char * phantom_buffer, unsigned long phantom_size, XCATPhantomCompress & compressor, tNode_t *root_node)
{
  unsigned long number_of_trials =  phantom_size/3;

  for(unsigned long j = 0; j < number_of_trials; j++){

    size_t random_voxel = j + phantom_size/3;

    XCATmat_t direct_material = phantom_buffer[random_voxel];
    
    //std::cerr << "voxel: " << random_voxel << "\tlabel: " << (int)direct_material << std::endl;
    
    // fetch rle material
    XCATmat_t compress_material;
    try {
      compress_material = compressor.getVoxelValue(random_voxel, root_node);
    } catch (Exception e){
      std::cerr << e.message << std::endl;
      exit(EXIT_FAILURE);
    }
    
    
    if(direct_material != compress_material){
      //error
      std::cerr << "error at voxel " << random_voxel << std::endl;
      std::cerr << "direct_material is  " << (int)direct_material << " and compress_material is " << (int)compress_material << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  
  std::cout << number_of_trials << " voxels compare correctly." << std::endl;
  
}

//
// 'Lookup voxel values' speed comparison
//
void speed_test(unsigned char * phantom_buffer, unsigned long phantom_size, XCATPhantomCompress & compressor, tNode_t *root_node)
{
  clock_t the_time = 0;
  clock_t direct_time = 0;
  clock_t compress_time = 0;

  srandom(time(NULL));

  unsigned long number_of_trials =  phantom_size/1000;

  for(unsigned long j = 0; j < number_of_trials; j++){

    size_t random_voxel = (unsigned long) floor(number_of_trials * (random()/(float)RAND_MAX));
    
    while(random_voxel > phantom_size)
      random_voxel = random() * (number_of_trials/RAND_MAX);
    
    // fetch material directly
    the_time = clock();
    XCATmat_t direct_material = phantom_buffer[random_voxel];
    direct_time += clock() - the_time;

    //std::cerr << "voxel: " << random_voxel << "\tlabel: " << (int)direct_material << std::endl;
    
    // fetch rle material
    XCATmat_t compress_material;
    try {
      the_time = clock();
      compress_material = compressor.getVoxelValue(random_voxel, root_node);
      compress_time += clock() - the_time;
    } catch (Exception e){
      std::cerr << e.message << std::endl;
      exit(EXIT_FAILURE);
    }
      
    if(direct_material != compress_material){
      //error
      std::cerr << "error at voxel " << random_voxel << std::endl;
      std::cerr << "direct_material is  " << (int)direct_material << " and compress_material is " << (int)compress_material << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  
  std::cout << "lookup direct:\t\t" << ((float)(direct_time)/CLOCKS_PER_SEC)/number_of_trials << " seconds/voxel." << std::endl;
  std::cout << "lookup compressed:\t" << ((float)(compress_time)/CLOCKS_PER_SEC)/number_of_trials << " seconds/voxel." << std::endl;
  std::cout << "over " << number_of_trials << " voxels" << std::endl;

}

