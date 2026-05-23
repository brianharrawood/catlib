/**
 * \file InputParams.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for the InputParams class.
 *
 */
#ifndef __InputParam_hh
#define __InputParam_hh

#include "ParamItem.hh"
#include "ParamTemplates.hh"

#include <vector>
#include <string>
#include <math.h>
#include <iostream>
#include <sstream>

// only 8 nested includes
#define INCLUDE_RECURSION_MAX 8

namespace catlib {

  /// \class InputParams
  /// The InputParams class is a base class container for parameters
  /// which may be transferred from the command line or read from a
  /// file. A unique application class is derived from this class to
  /// provide the specific tags and data required by the app.
  ///
  /// Parameters are templated into pairs of tag labels and values; e.g.;
  ///
  /// radius: 14.3
  ///
  /// Mechanisms are provided for lists of tags or defining structures
  /// as tag values; e.g.;
  ///
  /// element: {oxygen, 8, 15.999} 
  ///
  /// The parameters may be serialized and transferred between
  /// applications to assure consistency among many processes in a
  /// distributed environment.
  ///
  class InputParams
  {
  public:

    /// Constructor loads the template vector
    ///\param user_templates A vector of ParamTemplate
    //
    InputParams(const std::vector<ParamTemplate> user_templates);
    
    virtual ~InputParams();

    /// Reads params from a ''tag'' file and store the params
    ///\param param_file Name of the param file
    void loadParams(std::string &param_file);

    /// Permits overwriting the stored params with params in the param_stream
    ///\param param_stream A stream of replacement parameters 
    void loadOverrideParams(std::string &param_stream);

    /// Checks to see if a parameter is set in the object
    ///\param tag_id The tag label of interest
    ///\return true if the tag has a value
    bool isParamSet(const char * tag_id);

    /// Gets value of the data from a tag
    ///\param tag The tag label
    ///\param param_data The variable to hold the tag value
    ///\return 0 on success, may throw exception
    ///
    int getParamData(std::string tag, bool  &param_data);
    ///\overload int getParamData(std::string tag, float &param_data);
    int getParamData(std::string tag, float &param_data);
    ///\overload int getParamData(std::string tag, int   &param_data);
    int getParamData(std::string tag, int   &param_data);
    ///\overload int getParamData(std::string tag, std::string &param_data);     
    int getParamData(std::string tag, std::string &param_data);
    ///\overload int getParamData(std::string tag, void* &param_data);
    int getParamData(std::string tag, void* &param_data);

    /// Gets value of the data from a tag if set, otherwise use default
    ///\param tag_id        The tag label
    ///\param variable      The variable to hold the tag value
    ///\param default_value The the value to store in variable if tag is not set
    /// may throw exception
    ///
    /// This has no effect on tag data stored in object
    void CheckSet(const char* tag_id, bool &variable, bool default_value);
    ///\overload     void CheckSet(const char* tag_id, float &variable, float default_value);
    void CheckSet(const char* tag_id, float &variable, float default_value);
    ///\overload     void CheckSet(const char* tag_id, int &variable, int default_value);
    void CheckSet(const char* tag_id, int &variable, int default_value);
    /// \overload     void CheckSet(const char* tag_id, std::string &variable, std::string default_value);
    void CheckSet(const char* tag_id, std::string &variable, std::string default_value);
    
    /// Gets the output directory
    ///\returns the name of the output directory
    ///
    /// The output directory is a the only tag provided by the base class
    std::string getOutputDir() { if(ParamsLoaded_) return output_dir_; else return(std::string("")); }

    /// Informational call to retieve the ParamFile name
    std::string getParamFileName();

    /// All stored tags are written to a stringstream
    ///\param ss stringstream to hold serialized tags
    ///
    void Serialize(std::stringstream &ss);
    /// All tags stored in a stringstream are read into the object
    ///\param ss stringstream to hold serialized tags
    ///
    void Unserialize(std::stringstream &ss);

    /// write the serialized stream to file
    /// File is named with the process id (PID) of this process
    void Dump();

    /// Make a SHA-256 hash the serialized stream
    /// Maybe used for creating a keykey
    ///\return hash_string
    std::string getParamKey();

    /// Virtual function for derived class tags templated with data_t
    /// set to user_T are directed to this function for parsing and
    /// storage. Called from private method load_app_param()
    ///\param line_ptr     The string with the tag:value
    ///\param tag_template The template which matches this tag
    ///\param my_location  A FileIndex to track where the tag was loaded from.
    virtual 
    void load_user_param(std::string line_ptr, ParamTemplate tag_template, FileIndex &my_location);

    /// Virtual function to permit application to check tags for consistency
    ///\return 0 if parameters are OK
    virtual 
    int   sanity_check();

    bool  ParamsLoaded_;

  private:  
    // member funcs
    int    parse_param_buffer(char * buffer);
    void   load_ctl_param(std::string line_ptr, ParamTemplate tag_template, int line_no);
    void   load_app_param(std::string line_ptr, ParamTemplate tag_template, int line_no, bool over_ride = false);
    char * get_token(char * line, const char * token);
    void   map_tag(std::string line, int line_no);
    void   loadParamsFile(std::string param_file);
    void   loadRegistry();
    
    // wildcard handling
    std::string to_regex_expression(const std::string& path);
    void        replace_all( std::string& source, char replace, const std::string& replacement);

    // member vars
    //static - cannot make this static!!!
    std::map   <std::string, ParamTemplate>  tag_registry_;  
    std::map   <std::string, std::vector<FileIndex> > param_location_; // where we read it
    std::map   <std::string, ParamItem*> param_map_;
    std::vector<ParamItem*>              param_vec_;
    std::vector<ParamTemplate> user_templates_;

    std::string master_file_name_;
    FileIndex   current_file_index_;
    int         IncludeRecursion_; // counter
    
    std::string  output_dir_;
    
    std::vector <FileIndex>  included_files_;

    std::stringstream serialized_params_;
    std::string       ParamKey_;

  };
  
};
#endif
