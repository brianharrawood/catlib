/**
 * \file SyncLock.hh
 * \author B Harrawood
 * \date May 2026
 *    *
 * \brief Define the pthread synchronous locking Class
 *
 *
 */
#ifndef _SYNC_LOCK_H_
#define _SYNC_LOCK_H_

#include <pthread.h>

// deprecated; use std::lock_guard

namespace catlib {
  
  ///\class SyncLock
  ///
  /// A simple class to establish locking/waiting with a mutex and a
  /// condition variable
  ///
  class SyncLock {
    
  public:
    // create
    SyncLock()
    {
      pthread_mutexattr_t mutex_attr;
      pthread_condattr_t  cond_attr;
      
      // initialize
      pthread_mutexattr_init(&mutex_attr);
      pthread_condattr_init(&cond_attr);
      
      pthread_mutex_init(&m_mutex_, &mutex_attr);  
      pthread_cond_init(&m_cond_, &cond_attr);  
      
      m_cond_var_ = 0;    
    }
    
    ~SyncLock()
    {
      Unlock();
      pthread_mutex_destroy(&m_mutex_);
      pthread_cond_destroy(&m_cond_);
    }
    
    // lock or block
    void Lock()
    {
      // obtain lock
      pthread_mutex_lock(&m_mutex_);  
      
      while(m_cond_var_)
	pthread_cond_wait(&m_cond_, &m_mutex_);
      
      m_cond_var_ = 1;
      
      pthread_mutex_unlock(&m_mutex_);   
    }
    
    void Unlock()
    {
      // release lock
      pthread_mutex_lock(&m_mutex_);  
      
      m_cond_var_ = 0;
      pthread_cond_signal(&m_cond_);  
      
      pthread_mutex_unlock(&m_mutex_);   
    }
    
  private:
    
    pthread_mutex_t m_mutex_;
    pthread_cond_t  m_cond_;
    int             m_cond_var_;
    
  };
  
};

#endif
