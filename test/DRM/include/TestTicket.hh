#ifndef _TestTicket_hh
#define _TestTicket_hh

#include <string>

//catlib
#include "Ticket.hh"

class TestTicket : public catlib::Ticket
{
public:

  TestTicket(int Number_Of_Tasks = -1);

  ~TestTicket();

  void   SetTaskNumber(int tn) { TaskNumber_ = tn; }
  int    GetTaskNumber() {return TaskNumber_;}

  void Reset() {
    TaskNumber_  = -1; 
  }

  void TicketToBuffer(char * tx_string);

  void BufferToTicket(char * tx_string);

  void GetTicketID(std::string& id_string);
  
  void InValidate();

  int IsValid();

private:

  // zero indexed
  int     TaskNumber_;
  int     NumberOfTasks_;  

};

#endif
