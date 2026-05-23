#ifndef _myParams_hh
#define _myParams_hh

//catlib
#include "InputParams.hh"
#include "ParamItem.hh"
#include "ParamTemplates.hh"
#include "Exception.hh"

enum party_t {
  D,
  R,
  I,
  U
};
  
struct Voter_t {
  std::string  name;
  bool         registered;
  party_t      party;
  std::string  county;
};

class myParams : public catlib::InputParams
{
public:

  myParams(const std::vector<catlib::ParamTemplate> my_templates) :
    catlib::InputParams(my_templates) {;}

  ~myParams() {;}

  std::vector<std::string> getNames()  { return the_names_; }
  std::vector<Voter_t>     getVoters() { return the_voters_; }

  // load user_T data types
  void load_user_param(std::string line_ptr, catlib::ParamTemplate tag_template, catlib::FileIndex &my_location);
  
  int sanity_check();

private:

  int load_names(std::string spec);
  std::vector<std::string> the_names_;

  int load_voters(std::string spec);
  std::vector<Voter_t> the_voters_;

};


#endif
