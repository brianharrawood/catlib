/**
 * \file TicketManager.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Define TicketManager Class for the DRM
 *
 *
 */
#ifndef _TicketManager_hh
#define _TicketManager_hh

#include "Ticket.hh"

namespace catlib {
  
  ///\typedef void(\*DRMCallbackFunc_t)(int, void\*);
  //
  /// This is the prototype for the DRM Callback used in RunTickets()
  //
  typedef void(*DRMCallbackFunc_t)(int, void*);

  
  ///\class TicketManager
  ///
  /// The TicketManager is a base class which manages tickets for the
  /// DistributedRunManager. In particular, the TicketManager must be
  /// able to get a new Ticket, increment a Ticket, and know how to
  /// run the Ticket.
  ///
  /// The DistributedRunManager is required to have a registered
  /// TicketManager before it may call RunTickets()
  ///
  class TicketManager 
  {
  public:

    /// TicketManager constructor
    //
    /// \param callback_func The function invoked from the HandleTicket method
    /// \param callback_arg  One of the args passed in the callbacl function
    ///
    TicketManager(catlib::DRMCallbackFunc_t callback_func, void * callback_arg) :
      CurrentTicket_(0), NumberOfTickets_(0), CallbackFunc_(callback_func), CallbackArg_(callback_arg)  {;}
    
    virtual ~TicketManager() {;}
    
    /// HandleTicket
    //
    /// \param new_ticket
    ///\return 0 on success, mat throw exception
    /// 
    /// This function is responsible for calling the callback_func
    ///
    virtual int      HandleTicket(Ticket * new_ticket) { return(0);}

    /// EndOfTicketAction
    //
    /// \param filename Log file of completed ticket
    /// \param completed_ticket The ticket just finished
    /// \return 0 on success, mat throw exception
    /// 
    /// This function is call by RunTickets() immediately after
    /// HandleTicket() has successfully completed. It is intended as
    /// an optional method for updating progress.
    ///
    virtual int      EndOfTicketAction(const std::string filename, Ticket * completed_ticket) { return(0);}
    
    /// IncrementTicket
    ///
    /// \param ticket Ticket to update to next ticket
    /// \return 0 when tickets are exhausted, 1 otherwise
    ///
    virtual int      IncrementTicket(Ticket * ticket) = 0;
    
    /// ResetTickets
    ///
    /// Set the CurrentTicket_ to 0, and what ever other task are
    /// required to start the tickets over again`
    ///
    virtual void     ResetTickets() = 0;
    
    /// GetNewTicket
    ///
    /// Get a pointer to a new Ticket
    ///
    virtual Ticket * GetNewTicket() {return(new Ticket);}
    
    int    CurrentTicket_;
    int    NumberOfTickets_;

    void            * CallbackArg_;
    DRMCallbackFunc_t CallbackFunc_;
  };
};


#endif
