#ifndef _TestInputParams_hh
#define _TestInputParams_hh

//catlib
#include "InputParams.hh"
#include "ParamItem.hh"
#include "ParamTemplates.hh"
#include "Exception.hh"

// user data types
struct PointSpec_t {
  std::string color;
  float       transparency;
  int         size;
};

class TestInputParams : public catlib::InputParams
{
public:

  TestInputParams(const std::vector<catlib::ParamTemplate> templates) :
    catlib::InputParams(templates) {;}

  ~TestInputParams() {;}

  std::vector<PointSpec_t> &getPointSpecs() { return point_specs_; }
  
  void load_user_param(std::string line_ptr, catlib::ParamTemplate tag_template, catlib::FileIndex &my_location);
  
  int sanity_check();

private:

  int load_point_spec(std::string spec);

  std::vector<PointSpec_t> point_specs_;
};


#endif
