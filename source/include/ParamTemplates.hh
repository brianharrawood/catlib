/**
 * \file ParamTemplates.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Header file for the ParamTemplates struct.
 *
 * This struct defines an Input Parameter Tag
 *
 */
#ifndef __ParamTempl__H
#define __ParamTempl__H

#include <vector>

namespace catlib {
  

  
  ///\struct data_t
  ///
  /// Enumerated Template Data Types
  ///\code
  /// string_T,
  /// int_T,
  /// float_T,
  /// bool_T,
  /// user_T,
  /// no_T,
  ///\endcode
  enum data_t {
    string_T,
    int_T,
    float_T,
    bool_T,
    user_T,
    no_T,
  };
  
  ///\struct tag_t
  ///
  /// Enumerated Template types
  ///\code
  /// ctl_T,
  /// lib_T,
  /// app_T,
  ///\endcode
  enum tag_t {
    ctl_T,
    lib_T,
    app_T,
  };
  
  ///\struct ParamTemplate
  ///
  /// a template for InputParam tags
  /// \code
  /// char	tagID[64];
  /// data_t    data_type;
  /// bool	unique;   // only one of these tags is allowed
  /// tag_t     tag_type; // control tags vs application tags
  /// \endcode
  /// data_t has built-ins for bool, int, float and string. If it is
  /// set to user_T, the application must parse and store value
  ///
  struct ParamTemplate {
    char	tagID[64];
    data_t      data_type;
    bool	unique;   // only one of these tags is allowed
    tag_t       tag_type; // control tags vs application tags
  };

  const std::vector<ParamTemplate> the_templates =
    {
      // InputParameter Control Tags
      ParamTemplate{"include", string_T, false, ctl_T},
      ParamTemplate{"output_dir", string_T, true, lib_T},
    };
  
};

#endif

