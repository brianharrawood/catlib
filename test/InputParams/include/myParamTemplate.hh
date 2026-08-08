#ifndef __myParamTempl__H
#define __myParamTempl__H

#include "ParamTemplates.hh"

using namespace catlib;

// PointOnLine allows a single float value

// NumberOfPoints allows a single int value

// Names allows mutiple names to be input (single=false) so user must write the handler (data_t=user_T)
// see MyParams::load_names()

// Voter allows multiple voters *and* has a data struct to be filled in, therefore single=false and data_t=user_T
// see MyParams::load_voter()

const std::vector<ParamTemplate> my_templates =
  {
    // DRM test
    // tag name            data_t   single tag_t
    ParamTemplate{"PointOnLine",        float_T, true,  app_T},  // floating point type
    ParamTemplate{"NumberOfPoints",       int_T, true,  app_T},  // integer type
    ParamTemplate{"Names",               user_T, false, app_T},  // string type (false == allow multiple, so app_T means user writes hander)
    ParamTemplate{"Voter",               user_T, false, app_T},  // string type (false == allow multiple, so app_T means user writes hander)
  };

#endif

