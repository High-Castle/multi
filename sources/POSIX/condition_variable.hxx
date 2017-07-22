#ifndef MULTI_CONDITION_VARIABLE_HXX
#define MULTI_CONDITION_VARIABLE_HXX

// GreenTree 2017 highcastle.cxx@gmail.com , MIT

#include <system_error>
#include <type_traits>
#include <utility>
#include <chrono>

#include "../../mutex.hxx"
#include <pthread.h> 

namespace multi
{    
    enum struct cv_status 
    {  
        no_timeout ,
        timeout
    } ;
    
    struct condition_variable final
    {
        using native_handle_type = pthread_cond_t * ;
        
        condition_variable () 
        { 
            if ( int err = pthread_cond_init( &cv_ , nullptr ) ) 
                throw std::system_error{ err , std::system_category() } ;
        }
        
        condition_variable ( condition_variable const & ) = delete ;
        condition_variable& operator = ( condition_variable const& ) = delete ;
        
        ~ condition_variable () 
        { 
            pthread_cond_destroy( &cv_ ) ;
        }
        
        void notify_one () noexcept 
        { 
            pthread_cond_signal( &cv_ ) ;
        }
        
        void notify_all () noexcept 
        { 
            pthread_cond_broadcast( &cv_ ) ;
        }
        
        void wait ( unique_lock< mutex >& lock )
        {
            /* pthread_cond_wait atomically releases the associated mutex lock before blocking, 
             * and atomically acquires it again before returning. */
            if ( int err = pthread_cond_wait ( &cv_ , lock.mutex() -> native_handle() ) )    
                throw std::system_error{ err , std::system_category() } ; 
        }
        
        template < class Predicate >
        void wait( unique_lock< mutex >& lock, Predicate pred )
        {
            while ( ! pred () ) 
                wait( lock ) ;
        }
        
        template< class Clock, class Duration >
        cv_status wait_until( unique_lock< mutex >& lock,
                              const std::chrono::time_point< Clock , Duration >& timeout_time )
        {
            timespec wait_until = to_sys_timepoint( timeout_time ) ;
            int err = pthread_cond_timedwait( &cv_ , lock.mutex() -> native_handle() , &wait_until ) ;
            
            if ( ! err ) 
                return cv_status::no_timeout ;

            if ( err == ETIMEDOUT )
                return cv_status::timeout ; 
                
            throw std::system_error{ err , std::system_category() } ; 
        }
        
        template< class Clock, class Duration , class Predicate >
        bool wait_until( unique_lock< mutex >& lock,
                              const std::chrono::time_point< Clock, Duration >& timeout_time ,
                              Predicate pred )
        {
            timespec wait_until = to_sys_timepoint( timeout_time ) ;
            while ( ! pred () ) {
                if ( int err = pthread_cond_timedwait( &cv_ , lock.mutex() -> native_handle() , &wait_until ) ) 
                {
                    if ( err == ETIMEDOUT )
                        return false ; 
                    
                    throw std::system_error{ err , std::system_category() } ; 
                }   
            }
            return true ;
        }
        
        template< class Rep, class Duration, class Predicate >
        bool wait_for( unique_lock< mutex >& lock,
                       const std::chrono::duration< Rep , Duration >& duration )
        {
            return wait_until( lock , std::chrono::system_clock::now() + duration ) 
                            == cv_status::no_timeout ;
        }
        
        template< class Rep, class Duration, class Predicate >
        bool wait_for( unique_lock< mutex >& lock,
                         const std::chrono::duration< Rep , Duration >& duration,
                         Predicate pred )
        {
            return wait_until( lock , std::chrono::system_clock::now() + duration , std::move( pred ) ) ;
        }       
        
        native_handle_type native_handle () noexcept /* not const */ 
        {  
            return &cv_ ;
        }
        
        private :    
            template < class Clock , class Duration >
            static timespec to_sys_timepoint ( const std::chrono::time_point< Clock, Duration >& src )
            {
                using namespace std::chrono ;
                auto sys_time_point = std::is_same< Clock , system_clock >::value ? src
                                            : system_clock::now() + ( src - Clock::now() ) ;
                
                auto duration_since_epoch = sys_time_point.time_since_epoch() ;
                
                time_t sec = duration_cast< seconds >( duration_since_epoch ).count() ;
                long nsec = duration_cast< nanoseconds >( duration_since_epoch 
                                           - duration_cast< seconds >( duration_since_epoch ) ).count() ;
                
                return { sec , nsec } ;   
            }
            pthread_cond_t cv_ ;
    } ;
}

#endif // MULTI_THREAD_HXX
