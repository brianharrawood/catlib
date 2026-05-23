#ifndef __TestParamTempl__H
#define __TestParamTempl__H

#include "ParamTemplates.hh"

using namespace catlib;

const std::vector<ParamTemplate> DRMtest_templates =
  {
    // DRM test
    {"PointsPerTicket", float_T, true, app_T},     // number of random events per ticket (task)
    {"NumberOfTickets",  int_T, true, app_T},      // number of tickets to complete
    {"PointSpecs",  user_T, false, app_T},         // testing, sorry no graphics
  };

#endif

