#ifndef DRMTEST_H
#define DRMTEST_H

#include <getopt.h>
#include <string>
#include <iostream>
#include <filesystem>

// catlib
#include "DistributedRunManager.hh"
#include "FileIO.hh"
#include "Exception.hh"

#include "TestTicketManager.hh"
#include "TestInputParams.hh"
#include "TestParamTemplate.hh"

// command line arguments
struct cmd_input {
  std::string  parameter_file; // for masterd only
  int          port_number;    // client/server port
  bool         restart_flag;   // start from recovery file
};
typedef struct cmd_input CmdLineOptions_t;

int  parse_command_line(int ac, char ** av, CmdLineOptions_t &cmd_options);
void do_an_estimate(int ticket, void * additional_arg);
void get_random_point(double *x, double *y);
void write_result(double pi_estimate, int ticket_number, std::string output_directory);
void report_results(TestInputParams * inputParams, float seconds);
void usage();

#endif
