/**
 * \file XCATPhantomCompress.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Define XCATPhantomCompress Class
 *
 *
 */
#ifndef _XCAT_PHANTOM_COMPRESS_
#define _XCAT_PHANTOM_COMPRESS_

#include <string>
#include <vector>
#include <map>

#include "Exception.hh"

// keys per b-tree_node
#define MAX_KEYS 32

// Make this a string < 31 chars
// for header notice
// 1.0 initial issue
// 1.1 preserve null pointers at end of branches after relocation
#define RLE_FILE_NOTICE "XCAT RLE/B-tree V 1.1"

namespace catlib {

  typedef unsigned char XCATmat_t;

  struct tree_node;
  struct tree_key;
  
  ///\struct tree_node
  /// B-Tree node to contain tree_keys
  ///\code
  /// tree_node * parent;
  /// int         key_count;
  /// tree_key  * keys[MAX_KEYS];
  ///\endcode
  struct tree_node {
    tree_node * parent;
    int         key_count;
    tree_key  * keys[MAX_KEYS];
  };
  typedef struct tree_node tNode_t;
  
  ///\struct tree_key
  /// linked list RLE info
  ///\code
  /// unsigned long min_voxel;
  /// unsigned long max_voxel;
  /// unsigned int  voxel_value;
  /// tree_node * r_child;
  /// tree_node * l_child;
  ///\endcode
  struct tree_key {
    unsigned long min_voxel;
    unsigned long max_voxel;
    unsigned int  voxel_value;
    tree_node * r_child;
    tree_node * l_child;
  };
  typedef struct tree_key tKey_t;

  ///\struct file_header
  /// File header for compressed files
  ///
  ///\code
  /// char notice[32];
  /// tNode_t    * root_node;
  /// tNode_t    * first_node;
  /// unsigned int key_offset;
  ///\endcode
  struct file_header {
    char notice[32];
    tNode_t    * root_node;
    tNode_t    * first_node;
    unsigned int key_offset;
  };

  enum input_formats {
    RawInput = 0,
    CmpInput = 1
  };
    
  ///\class XCATPhantomCompress
  ///
  /// This is a relocate-able rle/btree compression of an XCAT
  /// Phantom. First the phantom is compressed using RLE then the RLE
  /// keys are stored in a b-tree for fast retrieval The tree_keys and
  /// btree tree_nodes are concatenated into a buffer for relocation
  /// (marshaling). The pointers in that buffer are 'relocated' to point
  /// to the new addresses in that buffer.
  ///
  /// The relocated (compressed) buffer may be stored as a file
  /// (WriteCompressedFile) and later read by this object.
  ///
  /// This class manages all memory allocated for itself. N.B.: the class must
  /// persist to preserve memory allocations.
  ///
  class XCATPhantomCompress {
    
  public:
    /// constructor
    ///
    /// Create an XCATPhantomCompress object
    //
    XCATPhantomCompress();

    ~XCATPhantomCompress();

    /// CompressFile
    ///
    /// Load in a phantom file and compress it in memory
    ///
    ///\param XCATFile Path to the phantom file
    ///\return Returns pointer to head node of the B-Tree
    ///
    /// This method loads a file into the object and produces a
    /// *compressed buffer* of that file. This is a necessary first
    /// step before any other methods may be called. The *compressed
    /// buffer* is actually a B-Tree structure marshaled into a
    /// contiguous buffer
    ///
    tNode_t * CompressFile(std::string XCATFile);
    
    /// getVoxelValue
    ///
    /// Retrieve the value of a voxel
    ///
    ///\param voxel_id The voxel number to retrieve
    ///\param root     Pointer to the root of the B-Tree to search
    ///\return Returns the value of the voxel
    ///
    unsigned int getVoxelValue(unsigned long voxel_id, tNode_t * root = 0);

    /// getCompressedBuffer
    ///
    /// Get a pointer to the object's *compressed buffer*. Object manages
    /// this memory.
    ///
    ///\param compressed_size Pointer to length of the *compressed buffer* (bytes)
    ///\return Pointer to the object's *compressed buffer*
    ///
    /// The *compressed buffer* (in the object) may be used in place
    /// or it may be copied into a new memory location. The caller
    /// manages memory at that location. Before it can be used at the
    /// new location, the application must call
    /// RelocateCompressionBuffer to adjust pointers in the buffer.
    ///
    unsigned char * getCompressedBuffer(unsigned long * compressed_size)
    {
      *compressed_size = compressed_size_;
      return compressed_buffer_;
    }

    /// RelocateCompressedBuffer
    ///
    /// Adjust the pointers in the *compressed buffer* to new location
    ///
    ///\param new_buffer_addr Pointer to the new address of the buffer
    ///\returns Pointer to the new root node of the B-Tree
    ///
    /// The *compressed buffer* is actually a B-Tree structure which
    /// contains internal pointers to tree nodes. The *compressed
    /// buffer* may be copied into a newly allocated location, but
    /// then it will need to have the internal pointers adjusted to
    /// that location. This function performs that adjustment and
    /// returns the new address of the root node.
    ///
    tNode_t * RelocateCompressionBuffer(unsigned char * new_buffer_addr);

    /// WriteCompressedFile
    ///
    /// Write the *compressed buffer* to a file for later use.
    ///
    /// The file input with the CompressFile() method is written back
    /// out to the same path, but with an '.rle' extension.
    ///
    size_t  WriteCompressedFile();

    /// WriteDecompressedFile
    ///
    /// Write the *compressed buffer* in ''raw'' format.
    ///
    /// The file input with the CompressFile() method is written back
    /// out to the same path, but without a compression
    /// extension. This may be used to decompress .rle or .gz files.
    ///
    size_t  WriteDecompressedFile();

    // raw phantom buffer - just for testing!!!
    // N.B. XCATFile may be an rle compressed file
    unsigned char * getPhantomBuffer(unsigned long * phantom_size)
    {
      *phantom_size = phantom_size_;
      return phantom_buffer_;
    }

    /// GetMaterialMap
    ///
    /// Return a map of the voxel values
    ///
    ///\returns Pointer to the map
    ///
    /// The map contains each voxel value along with the number of
    /// times it occurs in the *compressed buffer*.
    ///
    std::map<XCATmat_t, long> * GetMaterialMap();
    
    // test first 64 bytes of file
    // returns true if the file is XCATPhantomCompressed
    // false otherwise


    /// TestFile
    ///
    /// Check to see if file was created by this class.
    ///
    ///\param file_name File to test. File is NOT loaded into a *compressed buffer*
    ///\return true If file was compressed with this version of the class
    ///
    /// Will throw an exception if there is a version difference
    //
    bool TestFile(std::string file_name);

    /// TestHeader
    ///
    /// Check to see if file header was created by this class.
    ///
    ///\param buf First 64 bytes of a file to test
    ///\return true If file was compressed with this version of the class
    ///
    /// Will throw an exception if there is a version difference
    //
    bool TestHeader(char  buf[64]) { return(check_header((unsigned char *)buf)); }
    
  private:
    void      reset();
    void      compress();
    void      rle_compress(unsigned char * phantom_buffer, size_t size, std::vector<tKey_t> & key_vec);

    // make b-tree
    tNode_t * mk_tree(tKey_t  * key_buf, size_t size, std::vector<tNode_t> &node_vec);
    tNode_t * add_key_to_node(tKey_t * key, tNode_t * node, std::vector<tNode_t> &node_vec);
    tNode_t * add_node_to_tree(tNode_t * current_node, std::vector<tNode_t> &node_vec);
    tNode_t * mk_node(std::vector<tNode_t> &node_vec);

    // import compressed file
    bool      check_header(unsigned char * buffer);
    void      import_phantom();

    std::string     PhantomFile_;
    
    unsigned char * phantom_buffer_;
    unsigned long   phantom_size_;
    
    unsigned char * compressed_buffer_;
    unsigned long   compressed_size_;
    
    unsigned int    num_keys_;
    unsigned int    num_nodes_;

    tNode_t *       node_buffer_;
    tKey_t  *       key_buffer_;
    
    unsigned int    key_offset_; // bytes
    tNode_t *       root_;

    enum input_formats input_type_;

    std::map<XCATmat_t, long> material_map_;
  };
  
};

#endif
