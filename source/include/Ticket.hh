/**
 * \file Ticket.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Define Ticket Class for the DRM
 *
 *
 */
#ifndef _Ticket_hh
#define _Ticket_hh

#include <string>

#define TICKET_BUFFER_LEN 1024

namespace catlib {
  
  ///\class Ticket
  ///
  /// The Ticket class is a base class for Tickets used in the
  /// DistributedRunManager class. The information encapsulated in a
  /// ticket currently is the ticket_id, a simple integer. 
  ///
  /// Classes derived from the base class are capable of supporting
  /// more complex ticket information, such as structs.
  ///
  /// The Ticket class is responsible for marshalling the ticket's
  /// information into a string for transmission on the network and
  /// similary, for converting a network string back to ticket
  /// information.
  class Ticket 
  {
  public:
    
    Ticket() {;}
    virtual ~Ticket() {;}
    
    /// A virtual method to return the ticket_id as a string
    /// 
    /// \param ticket_id is a string with the ticket_id
    ///
    virtual void GetTicketID(std::string& ticket_id) {;}
    
    /// A virtual method to marshall a ticket's information into a string
    /// 
    /// \param tx_string is the resulting string
    ///
    virtual void TicketToBuffer(char * tx_string) {;}
    
    /// A virtual method to fetch a ticket's information from a string
    /// 
    /// \param tx_string is the string containing the ticket information
    ///
    virtual void BufferToTicket(char * tx_string) {;}
    
    /// A virtual method to invalidate a ticket
    virtual void InValidate() {;}
    
    /// A virtual method to check if a ticket is invalid
    virtual int IsValid() {return(0);}
  };
};
#endif
