#include "XCATPhantomCompress.hh"
#include "FileIO.hh"
#include <fcntl.h>
#include <algorithm>
#include <cassert>

using namespace catlib;

XCATPhantomCompress::XCATPhantomCompress():phantom_file_buffer_(0), phantom_file_size_(0), num_keys_(0), key_buffer_(0), num_nodes_(0), node_buffer_(0), compressed_buffer_(0), compressed_size_(0), raw_buffer_(0), raw_size_(0), key_offset_(0), root_(0)
{
  ;
}

XCATPhantomCompress::~XCATPhantomCompress()
{
  delete [] phantom_file_buffer_;
  delete [] compressed_buffer_;
  delete [] raw_buffer_;
}

void XCATPhantomCompress::reset()
{
  material_map_.clear();

  delete [] phantom_file_buffer_;
  delete [] compressed_buffer_;
  delete [] raw_buffer_;

  phantom_file_buffer_ = 0;
  phantom_file_size_   = 0;
  compressed_buffer_   = 0;
  compressed_size_     = 0;
  raw_buffer_          = 0;
  raw_size_            = 0;
  num_keys_            = 0;
  key_buffer_          = 0;
  num_nodes_           = 0;
  node_buffer_         = 0;
  key_offset_          = 0;
  root_                = 0;
}

// a file must be loaded before other methods are called
bool XCATPhantomCompress::LoadFile(std::string XCATFile, bool do_compression)
{
  reset();
  
  PhantomFile_ = XCATFile;
  
  // read file
  catlib::FileIO filer;
  phantom_file_size_ = filer.read_file(PhantomFile_, &phantom_file_buffer_);

  // the buffer maybe either raw or rle_compressed
  bool is_already_XCATcompressed = check_header(phantom_file_buffer_);
  
  if(is_already_XCATcompressed){
    // phantom_file_buffer is compressed
    // relocate phantom_file_buffer_ to compress_buffer_
    import_phantom();
    input_type_ = CmpInput;
  }
  else {
    // phantom_file_buffer is raw
    // compress regardless of 'do_compression' for material map
    compress();
    input_type_ = RawInput;
  }

  // for uncompressed raw
  //
  if(!do_compression && (input_type_ == RawInput)){
    // move phantom_file_buffer to raw_buffer
    raw_buffer_ = phantom_file_buffer_;
    raw_size_   = phantom_file_size_;
    phantom_file_buffer_ = 0;	
  }
  else if(!do_compression && (input_type_ == CmpInput)){
    // Want raw buffer but input is compressed.  Uncompress phantom to
    // raw: this sets raw_buffer_ and raw_buffer_size_
    rle_uncompress();
   }
    
  return(true);
}

unsigned int
XCATPhantomCompress::getVoxelValue(unsigned long voxel_id, tNode_t * root)
{
  if(root == 0)
    root = root_;

  tNode_t * next_node = root;
  bool      continue_while = false;
  
  while(next_node != nullptr){

    for(int i = 0; i < next_node->key_count; i++) {
      // this is my node!
      if((voxel_id <= next_node->keys[i]->max_voxel) &&
	 (voxel_id >= next_node->keys[i]->min_voxel))
	return(next_node->keys[i]->voxel_value);
    }

    continue_while = false;
    
    for(int i = 0; i < next_node->key_count; i++) {
      // this is next node
      if(voxel_id < next_node->keys[i]->min_voxel){
	next_node = next_node->keys[i]->l_child;
	continue_while = true;
	break;
      }
    }

    // key is bigger than all keys on this node
    // this is next node
    if(!continue_while)
      next_node = next_node->keys[next_node->key_count - 1]->r_child;
  }

  // exception!
  std::ostringstream stringStream;
  stringStream << "Voxel " << voxel_id << " not found!" << std::endl;
  
  Exception_t e;
  e.message = stringStream.str();
  throw (e);
  
  // for the compiler - we will not reach this
  return(0);
}

// compress phantom_file_buffer_
// into compressed_buffer_
void
XCATPhantomCompress::compress()
{
  // compresses and creates b-tree, originally in vector containers,
  // then relocated into a compress_buffer_
  std::vector<tKey_t> key_vector;
  key_vector.reserve(phantom_file_size_/100);
  
  rle_compress(phantom_file_buffer_, phantom_file_size_, key_vector);
  
  num_keys_ = key_vector.size();
  key_buffer_ =  (tKey_t *)key_vector.data();

  // make b-tree - creates node_address_ and puts keys into nodes
  // and sets root_index_ needed for search

  // reserving space needed to keep pointers aligned to nodes
  unsigned int approximate_num_nodes = 100 + num_keys_/(MAX_KEYS / 2);
  std::vector<tNode_t> node_vector;
  node_vector.reserve(approximate_num_nodes);

  root_ = mk_tree(key_buffer_, num_keys_, node_vector);

  num_nodes_ = node_vector.size();
  node_buffer_ = (tNode_t*)node_vector.data();

  // this is a bit of a hack...
  // not sure if it will ever happen.
  if(num_nodes_ > approximate_num_nodes) {
    // re-make tree! We underestimated the vector size required.
    node_vector.clear();

    node_vector.reserve(num_nodes_);

    root_ = mk_tree(key_buffer_, num_keys_, node_vector);

    num_nodes_ = node_vector.size();
    node_buffer_ = (tNode_t*)node_vector.data();
  }
  
  //
  // move the compressed memory out of the vector containers
  // and into compressed_buffer_
  //
  compressed_size_ = (num_keys_ * sizeof(tKey_t)) + (num_nodes_ * sizeof(tNode_t));
  compressed_buffer_ = new unsigned char[compressed_size_];

  if(compressed_buffer_ == 0){
    // exception
    AllocationException(__FILE__, __LINE__, compressed_size_);
  }
  
  key_offset_ = num_nodes_ * sizeof(tNode_t);
  memcpy(compressed_buffer_, (void *)node_buffer_, key_offset_);
  memcpy(&compressed_buffer_[key_offset_], (void *)key_buffer_, num_keys_ * sizeof(tKey_t));

  // relocate pointers in compression_buffer_
  root_ = RelocateCompressionBuffer(compressed_buffer_);

  // relocated
  node_buffer_ = (tNode_t *)compressed_buffer_;
  key_buffer_  = (tKey_t *)(compressed_buffer_ + key_offset_);

}

// walk through raw phantom and make the rle keys
void
XCATPhantomCompress::rle_compress(unsigned char * phantom_buffer, size_t buffer_size, std::vector<tKey_t> &key_vector)
{
  // start the loop
  size_t     i;
  tKey_t     rle_key; // tree node and rle code are same data struct
  
  memset(&rle_key, 0, sizeof(tKey_t));
  rle_key.voxel_value = phantom_buffer[0];
  
  for(i = 1; i < buffer_size; i++){

    if(rle_key.voxel_value ==  phantom_buffer[i]){
      continue;
    }
    
    // save node
    rle_key.max_voxel = i - 1;
    key_vector.push_back(rle_key);
    
    // resume counting
    memset(&rle_key, 0, sizeof(tKey_t));
    rle_key.min_voxel = i;
    rle_key.voxel_value = phantom_buffer[i];
  }
  // end of loop - push last node
  rle_key.max_voxel = i - 1;    
  key_vector.push_back(rle_key);
}

// make the b-tree to contain the rle_keys
tNode_t *
XCATPhantomCompress::mk_tree(tKey_t * key_buf, size_t size, std::vector<tNode_t> &node_vector)
{
  // pointer to current node
  tNode_t * current_node = mk_node(node_vector);
  
  for(unsigned long i = 0; i < size; i++)
    current_node = add_key_to_node(&key_buf[i], current_node, node_vector);
  
  //
  // find root
  while(current_node->parent != nullptr)
    current_node = current_node->parent;
  
  tNode_t * root = current_node;

  return(root);
}

// load keys into nodes until they overflow
tNode_t *
XCATPhantomCompress::add_key_to_node(tKey_t * key, tNode_t * node, std::vector<tNode_t> &node_vector)
{
  // if there are children - add to child on right

  // node is not full - easy
  if(node->key_count < MAX_KEYS){
    node->keys[node->key_count++] = key;
    return(node);
  }
  
  // else node full - MAX_KEYS exceeded - add a node
  tNode_t * fresh_node = add_node_to_tree(node, node_vector);

  return(add_key_to_node(key, fresh_node, node_vector));
}

// creates new node(s) and updates key info
tNode_t *
XCATPhantomCompress::add_node_to_tree(tNode_t * current_node, std::vector<tNode_t> &node_vector)
{
  // new nodes needed!
  tNode_t *parent;
  
  // maybe a new parent
  if(current_node->parent != 0)
    parent = current_node->parent;
  else {
    // current node will need a parent
    current_node->parent = mk_node(node_vector);
    parent = current_node->parent;
  }
  
  // certainly a new peer - for right side of tree
  tNode_t * peer = mk_node(node_vector);

  // promote median key from current node to parent
  tKey_t * median_key = current_node->keys[MAX_KEYS/2];

  // this call may recurse
  // and create additional parents as we go
  tNode_t *new_parent = add_key_to_node(median_key, parent, node_vector);
  
  // utill finally this
  peer->parent = new_parent;
  
  // set child pointers
  median_key->l_child = current_node;
  median_key->r_child = peer;

  // rm median key from current_node
  current_node->keys[MAX_KEYS/2] = 0;
  
  // move keys above median from current to peer
  int excess_keys = MAX_KEYS - MAX_KEYS/2 - 1;

  memcpy(&peer->keys, &current_node->keys[MAX_KEYS/2 + 1],
	 excess_keys * sizeof(tKey_t*));

  memset(&current_node->keys[MAX_KEYS/2 + 1], 0,  excess_keys * sizeof(tKey_t*));

  // the -1 is for the median key
  current_node->key_count = current_node->key_count - excess_keys - 1;
  peer->key_count = excess_keys;
  
  return(peer);
}

// new b-tree node - vector allocates memory
tNode_t * XCATPhantomCompress::mk_node(std::vector<tNode_t> &node_vector)
{
  tNode_t node;
  memset(&node, 0, sizeof(tNode_t));
  node_vector.push_back(node);

  return(&(node_vector.back()));
}

// copy relocation_buffer_ to relocation_addr and modify internal
// pointers. Must set node_buffer_ and key_buffer_ BEFORE using
tNode_t * XCATPhantomCompress::RelocateCompressionBuffer(unsigned char * relocation_addr)
{
  //
  // pointer switch-a-roo
  //

  // offsets in existing buffer
  tNode_t * head_node = node_buffer_;
  tKey_t  * head_key  = key_buffer_;

  tNode_t * r_head_node = (tNode_t*)relocation_addr;
  tKey_t  * r_head_key  = (tKey_t*)(relocation_addr + key_offset_);

  //
  // step through relocation buffer @relocation_addr and modify
  // addresses
  //

  // first the nodes
  tNode_t * r_node_buf = (tNode_t *)relocation_addr;
  
  // for each node...
  for(unsigned int n = 0; n < num_nodes_; n++){
    tNode_t * the_node = &r_node_buf[n];
    the_node->parent = r_head_node + (the_node->parent - head_node);

    for(unsigned int k = 0; k < the_node->key_count; k++)
      the_node->keys[k] = r_head_key + (the_node->keys[k] - head_key);
  }
  
  // and now each key...
  tKey_t * r_key_buf = (tKey_t *)(relocation_addr + key_offset_);

  for(unsigned int k = 0; k < num_keys_; k++){
    tKey_t * the_key = &r_key_buf[k];
    if(the_key->r_child != nullptr)
      the_key->r_child = r_head_node + (the_key->r_child - head_node);
    if(the_key->l_child != nullptr)
      the_key->l_child = r_head_node + (the_key->l_child - head_node);
  }

  tNode_t * root_node = r_head_node + (root_ - head_node);
  root_node->parent = nullptr;
  
  return(root_node);
}

// writes a .rle XCATPhantomCompess file
// maybe pass in an output file name??
size_t XCATPhantomCompress::WriteCompressedFile()
{
  if(input_type_ == CmpInput){
    std::cerr << "Warning: input is in .rle format already!" << std::endl;
    std::cerr << "no action taken. " << std::endl;
    return(0);
  }

  // remove .rle .gz or .gzip from phantom_file
  std::string rle_file = PhantomFile_;

  // trim old file extensions
  size_t lastindex = rle_file.find_last_of(".");
  while(lastindex != std::string::npos){

    if((0 == rle_file.compare(lastindex, 3, ".gz")) || (0 == rle_file.compare(lastindex, 5, ".gzip"))){
      rle_file = rle_file.substr(0, lastindex);
    }
    else if(0 == rle_file.compare(lastindex, 4, ".rle")){
      // recompressing?? should rm .rle or .rle.gz
      rle_file = rle_file.substr(0, lastindex);
    }
    else
      break;
    
    lastindex = rle_file.find_last_of(".");
  }
  
  // add our new extension
  rle_file.append(".rle");

  // remove leading path - write to local dir
  lastindex = rle_file.find_last_of("/");
  if(lastindex != std::string::npos)
    rle_file = rle_file.substr((lastindex + 1), std::string::npos);

  struct file_header header;
  memset(&header, 0, sizeof(header));

  char notice[32] = RLE_FILE_NOTICE;
  memcpy(header.notice, &notice, 32);
  
  header.first_node  = (tNode_t*)node_buffer_;
  header.root_node   = root_;
  header.key_offset  = key_offset_;
  
  unsigned char * file_buffer = new unsigned char[compressed_size_ + sizeof(header)];

  if(file_buffer == 0){
    // exception
    AllocationException(__FILE__, __LINE__, compressed_size_ + sizeof(header));
  }

  memcpy(file_buffer, &header, sizeof(header));
  memcpy(&file_buffer[sizeof(header)], compressed_buffer_, compressed_size_);
  
  FileIO filer;
  size_t written = filer.write_file(rle_file, file_buffer, sizeof(header) + compressed_size_);

  delete[] file_buffer;
  
  return(written);
}

// writes a .rle XCATPhantomCompess file as raw phantom
size_t XCATPhantomCompress::WriteDecompressedFile()
{
  if(input_type_ != CmpInput){
    std::cerr << "Warning: input is already decompressed!" << std::endl;
    std::cerr << "no action taken. " << std::endl;
    return(0);
  }

  // remove .rle .gz or .gzip from phantom_file
  std::string rle_file = PhantomFile_;

  // trim old file extensions
  size_t lastindex = rle_file.find_last_of(".");
  while(lastindex != std::string::npos){

    if((0 == rle_file.compare(lastindex, 3, ".gz")) || (0 == rle_file.compare(lastindex, 5, ".gzip"))){
      rle_file = rle_file.substr(0, lastindex);
    }
    else if(0 == rle_file.compare(lastindex, 4, ".rle")){
      // recompressing?? should rm .rle or .rle.gz
      rle_file = rle_file.substr(0, lastindex);
    }
    else
      break;
    
    lastindex = rle_file.find_last_of(".");
  }
  
  //
  // open file and rip through the rle_keys
  int fd = open(rle_file.c_str(), (O_RDWR | O_CREAT | O_EXCL | O_LARGEFILE), 0644);

  if(fd < 0){
    // exception
    std::ostringstream stringStream;
    stringStream << "Error opening '" << rle_file << "'." << std::endl;
    stringStream << strerror(errno) << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    
    throw (e);
  }
  
  size_t total_file_size = 0;
  
  for(unsigned int i = 0; i < num_keys_; i++){
    tKey_t key = key_buffer_[i];

    long count = 1 + (key.max_voxel - key.min_voxel);
    unsigned char value = (unsigned char)key.voxel_value;

    unsigned char buf[count];

    memset(buf, value, count);

    long writ = write(fd, buf, count);

    if(writ != count){
      // exception
      std::ostringstream stringStream;
      stringStream << "Error writing to '" << rle_file << "'." << std::endl;
      stringStream << strerror(errno) << std::endl;

      Exception_t e;
      e.message = stringStream.str();
      
      throw (e);
    }
      
    total_file_size += writ;
  }

  close(fd);
  
  return(total_file_size);
}

// sets raw_buffer_ and raw_buffer_size_
void XCATPhantomCompress::rle_uncompress()
{
  // we must have a compressed file!
  assert(compressed_buffer_ != 0);

  // buffer_size is unknown beforehand, so we uncompress to a vector
  // and then write to buffer  
  std::vector<unsigned char> voxel_vector;
  voxel_vector.reserve(phantom_file_size_ * 20);
  
  size_t total_file_size = 0;
  long   total_count = 0;
  
  for(unsigned int i = 0; i < num_keys_; i++){
    tKey_t key = key_buffer_[i];

    long count = 1 + (key.max_voxel - key.min_voxel);
    unsigned char value = (unsigned char)key.voxel_value;

    voxel_vector.resize(voxel_vector.size()+count, value);
  }

  raw_size_ =  voxel_vector.size();
  raw_buffer_ = new unsigned char[raw_size_];

  if(raw_buffer_ == 0){
    // exception
    AllocationException(__FILE__, __LINE__, raw_size_);
  }

  std::copy(voxel_vector.begin(), voxel_vector.end(), raw_buffer_);

}

// test file header to see if the file was written by
// XCATPhantomCompress
bool XCATPhantomCompress::check_header(unsigned char * buffer)
{  
  struct file_header *header = (struct file_header *)buffer;
  
  char notice[32] = RLE_FILE_NOTICE;
  
  if(0 == strcmp(notice, header->notice))
    return(true);

  // maybe a version issue??
  // see if the first 16 characters agree
  if(0 == strncmp(notice, header->notice, 16)){
    // exception!
    std::ostringstream stringStream;
    stringStream << "Input file " << PhantomFile_ << " was compressed with '" << std::string(header->notice) << "'" << std::endl;
    stringStream << "Application expects '" << std::string(notice) << "'" << std::endl;
    
    Exception_t e;
    e.message = stringStream.str();
    throw (e);
  }

  // not an rle compressed buffer!
  return(false);
}

// import an XCATPhantomCompress .rle file
void XCATPhantomCompress::import_phantom()
{
  struct file_header *header = (struct file_header *)phantom_file_buffer_;

  // recreate addresses for incoming phantom from header
  key_offset_  = header->key_offset;
  root_        = header->root_node;
  node_buffer_ = header->first_node;
  key_buffer_  = (tKey_t *)(key_offset_ + (unsigned char *)node_buffer_);

  compressed_size_ = phantom_file_size_ - sizeof(struct file_header);
  compressed_buffer_ = new unsigned char[compressed_size_];

  num_nodes_   = key_offset_ / sizeof(tNode_t);
  num_keys_    = (compressed_size_ - key_offset_) / sizeof(tKey_t);
  
  if(compressed_buffer_ == 0){
    // exception
    AllocationException(__FILE__, __LINE__, compressed_size_);
  }
  
  memcpy(compressed_buffer_, phantom_file_buffer_ + sizeof(struct file_header), compressed_size_);

  // now relocate to compressed buffer addr
  root_ = RelocateCompressionBuffer(compressed_buffer_);

  // relocated
  node_buffer_ = (tNode_t *)compressed_buffer_;
  key_buffer_  = (tKey_t *)(compressed_buffer_ + key_offset_);

}

std::map<XCATmat_t, long> * XCATPhantomCompress::GetMaterialMap()
{
  material_map_.clear();

  // step through the keys and find unique labels and their voxel
  // count
  for(int i = 0; i < num_keys_; i++){

    // voxel label
    // cast unsigned int to XCATmat_t (unsigned char)
    XCATmat_t key = (XCATmat_t) key_buffer_[i].voxel_value;

    // number of these voxels - convert indices to count
    long num_voxels = 1 + key_buffer_[i].max_voxel - key_buffer_[i].min_voxel;
    
    if(auto found = material_map_.find(key); found != material_map_.end())
      // update voxel label
      material_map_[key] = num_voxels + found->second;
    
    else
      // new voxel label
      material_map_.insert({key, num_voxels});
  }
  
  return(&material_map_);
}


bool XCATPhantomCompress::TestFile(std::string file_name)
{
  unsigned char test_buffer[64];
  
  // read file
  catlib::FileIO filer;
  int rc = filer.read_file(file_name, test_buffer, 64L);

  // 32 is a magic number - sizeof header notice
  if(rc < 32){
    return(false);
  }

  return(check_header(test_buffer));
}
