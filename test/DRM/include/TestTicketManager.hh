#ifndef _TestTicketManager_hh
#define _TestTicketManager_hh

#include "TestTicket.hh"

class TestInputParams;

// catlib
#include "TicketManager.hh"

class TestTicketManager : public catlib::TicketManager
{

public:

  TestTicketManager(catlib::DRMCallbackFunc_t callback_func, TestInputParams * inputParams);

  ~TestTicketManager();

  int  HandleTicket(catlib::Ticket * new_ticket);
  
  int  EndOfTicketAction(const std::string output_name, catlib::Ticket * the_ticket);
  
  int  IncrementTicket(catlib::Ticket * ticket);

  void ResetTickets();

  catlib::Ticket * GetNewTicket();

private:
  // pointer to function to actually process the ticket
  TestInputParams * inputParams_;
  
  void (*run_ticket_)(int ticket_no);

};

#endif
