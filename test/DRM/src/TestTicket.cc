#include "TestTicket.hh"
#include <iostream>

TestTicket::TestTicket(int Number_Of_Tasks)
  : TaskNumber_(-1), NumberOfTasks_(Number_Of_Tasks)
{
  ;
}

TestTicket::~TestTicket()
{ 
  ;
}

void TestTicket::GetTicketID(std::string& the_id)
{
  
  char buffer[1024];

  sprintf(buffer, "task_%06d", TaskNumber_);
  
  the_id = (const char *)buffer;
}



void TestTicket::TicketToBuffer(char * tx_buffer)
{

  sprintf(tx_buffer, "%06d", TaskNumber_);

  return;
}

void TestTicket::BufferToTicket(char * tx_buffer)
{
  int task = 0;

  
  if(1 != sscanf(tx_buffer, "%d", &task) || (tx_buffer == 0)){
    // error - Throw Exception??
    std::cerr <<  "error scanning ticket: " << __FILE__ << ":" << __LINE__ << std::endl;
    return;
  }

  TaskNumber_ = task;
}

void  TestTicket::InValidate()
{
  TaskNumber_ = -1;
}

int TestTicket::IsValid()
{
  if(TaskNumber_  == -1)
    return(0);
  
  return(1);
}
