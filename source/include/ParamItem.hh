/**
 * \file ParamItem.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for the ParamItem Class.
 *
 * This class makes a single parameter item. The InputParams Class
 * makes a collection of these.
 *
 */
#ifndef _ParamItem_hh_
#define _ParamItem_hh_

#include "ParamTemplates.hh"

#include <string>
#include <map>
#include <iostream>
#include <sstream>
#include <algorithm>

namespace catlib {


  ///\struct FileIndex
  ///
  // helps track file parsing
  /// \code
  /// FileIndex(): file_name(" "), line_number(-1) {} // default values
  /// std::string file_name;
  /// int         line_number;
  /// \endcode
  struct FileIndex {
    FileIndex(): file_name(" "), line_number(-1) {} // default values
    std::string file_name;
    int         line_number;
  };
  
  ///\class ParamItem
  /// The ParamItem class is a base class container for the individual
  /// parameters contained in the InputParams class. The ParamItem is
  /// itself is a container to hold the tag_id (label), the value, its
  /// template, and the location from which it was read.
  ///
  /// This class is used by InputParams class and is not called by the
  /// application directly.
  ///
  class ParamItem
  {
  public:
    // create ParamItem from string (parsed from input file)
    ParamItem(std::string the_line, std::map<std::string, ParamTemplate> tag_registry, FileIndex location);
    
    // create ParamItem from string (parsed from input file) & known temlpate
    ParamItem(std::string the_line, ParamTemplate tag_template, FileIndex location);
    
    virtual ~ParamItem();
    
    std::string getTagID() {return tag_;}
    
    std::string getValue() {return value_;} // value(data) as string!
    
    data_t      getType()  {return data_type_;}

    std::string Serialize();
    
    ParamTemplate getTemplate() { return the_template_; }
    
    // see derived classes below
    virtual bool        getData(void *);
    virtual bool        getData(int &data);
    virtual bool        getData(float &data);
    virtual bool        getData(bool &data); 
    virtual bool        getData(std::string &data);

    // pubicly avail to derived classes
    // which convert 'value' to 'data'
    std::string value_;
    std::string str_data_;
    int         int_data_;
    float       flt_data_;
    bool        bool_data_;
    void      * void_data_;
    
  private:
    
    // member funcs
    void parse_line(std::string new_line);
    std::string trim(const std::string& str);
    
    // member vars
    std::string tag_;
    data_t      data_type_;
    std::string required_;
    ParamTemplate the_template_; // read only
    
    // track source of tag
    FileIndex location_;
    
  };
  
  //
  // derived classes
  //
  class ParamItemStr : public ParamItem {
  public:
    ParamItemStr(std::string the_line, std::map<std::string, ParamTemplate> tag_registry, FileIndex location):
      ParamItem(the_line, tag_registry, location) { ; }
    
    ParamItemStr(std::string the_line, ParamTemplate tag_template, FileIndex location):
      ParamItem(the_line, tag_template, location) { ; }
    
    bool getData(std::string &the_data) {
      //str_data_ = value_;
      the_data = value_;
      return (true);
    }
    bool getData(void *)     {return false;}
    bool getData(int &data)  {return false;}
    bool getData(float &data){return false;}
    bool getData(bool &data) {return false;} 
  };
  
  class ParamItemInt : public ParamItem {
  public:
    ParamItemInt(std::string the_line, std::map<std::string, ParamTemplate> tag_registry, FileIndex location):
      ParamItem(the_line, tag_registry, location) { ; }
    
    ParamItemInt(std::string the_line, ParamTemplate tag_template, FileIndex location):
      ParamItem(the_line, tag_template, location) { ; }
    
    bool getData(int & the_data) {
      int_data_ = std::stoi(value_);
      the_data = int_data_;
      return (true);
    }
    bool getData(void *)            {return false;}
    bool getData(std::string &data) {return false;}
    bool getData(float &data)       {return false;}
    bool getData(bool &data)        {return false;} 
  };
  
  class ParamItemFlt : public ParamItem {
  public:
    ParamItemFlt(std::string the_line, std::map<std::string, ParamTemplate> tag_registry, FileIndex location):
      ParamItem(the_line, tag_registry, location) { ; }
    ParamItemFlt(std::string the_line, ParamTemplate tag_template, FileIndex location):
      ParamItem(the_line, tag_template, location) { ; }
    
    bool getData(float & the_data) {
      flt_data_ = std::stof(value_);
      the_data = flt_data_;
      return (true);
    }
    bool getData(void *)            {return false;}
    bool getData(std::string &data) {return false;}
    bool getData(int &data)         {return false;}
    bool getData(bool &data)        {return false;} 
  };
  
  class ParamItemBool : public ParamItem {
  public:
    ParamItemBool(std::string the_line, std::map<std::string, ParamTemplate> tag_registry, FileIndex location):
      ParamItem(the_line, tag_registry, location) { ; }
    ParamItemBool(std::string the_line, ParamTemplate tag_template, FileIndex location):
      ParamItem(the_line, tag_template, location) { ; }
    
    bool getData(bool & the_data) {
      std::transform(value_.begin(), value_.end(), value_.begin(), tolower);
      if((value_.compare("true") == 0) || (value_.compare("yes") == 0)) 
	bool_data_ = true;
      else
	bool_data_ = false;
      
      the_data = bool_data_;
      return(true);  
    }
    bool getData(void *)            {return false;}
    bool getData(std::string &data) {return false;}
    bool getData(int &data)         {return false;}
    bool getData(float &data)       {return false;} 
  };
  
  class ParamItemVoid : public ParamItem {
  public:
    ParamItemVoid(std::string the_line, std::map<std::string, ParamTemplate> tag_registry, FileIndex location):
      ParamItem(the_line, tag_registry, location) { ; }
    ParamItemVoid(std::string the_line, ParamTemplate tag_template, FileIndex location):
      ParamItem(the_line, tag_template, location) { ; }
    
    bool getData(void * the_data) {
      void_data_ = the_data;
      return (false);
    }
    bool getData(std::string &data) {return false;}
    bool getData(int &data)         {return false;}
    bool getData(float &data)       {return false;} 
    bool getData(bool &data)        {return false;} 
  };
  
};

#endif
