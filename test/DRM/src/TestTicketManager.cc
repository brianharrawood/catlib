#include "TestTicketManager.hh"
#include "TestInputParams.hh"

TestTicketManager::TestTicketManager(catlib::DRMCallbackFunc_t callback_func, TestInputParams *inputParams):
  catlib::TicketManager(callback_func, (void*)inputParams)
{
  inputParams->getParamData("NumberOfTickets", NumberOfTickets_);
}

TestTicketManager::~TestTicketManager()
{
}

int TestTicketManager::HandleTicket(catlib::Ticket * new_ticket)
{
  TestTicket * my_ticket = (TestTicket *)new_ticket;

  int next_task = my_ticket->GetTaskNumber();
  
  CallbackFunc_(next_task, CallbackArg_);
  
  return(0);
}

int  TestTicketManager::EndOfTicketAction(const std::string output_name, catlib::Ticket *)
{
  // maybe used with callback to update progress
  return(0);
}

void TestTicketManager::ResetTickets()
{
  CurrentTicket_ = 0;
}

// Only runs in Master Process
// return 0 when tickets are exhausted
// 1 otherwise
int TestTicketManager::IncrementTicket(catlib::Ticket * ticket)
{
  TestTicket * next_ticket = (TestTicket *)ticket;
  
  while(CurrentTicket_ < NumberOfTickets_){
    
    next_ticket->SetTaskNumber(CurrentTicket_);

    CurrentTicket_++;

    return(1);

  } // end while
  
  return(0);
}

catlib::Ticket * TestTicketManager::GetNewTicket()
{
  TestTicket * return_ticket = new TestTicket();

  return(return_ticket);
}
