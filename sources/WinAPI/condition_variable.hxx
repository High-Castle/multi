#ifndef MULTI_CONDITION_VARIABLE_HXX
#define MULTI_CONDITION_VARIABLE_HXX

// GreenTree 2017 highcastle.cxx@gmail.com , MIT

#include <system_error>
#include <type_traits>
#include <utility>
#include <chrono>

#include "../../mutex.hxx"

#include <windows.h> 

namespace multi
{    
    enum struct cv_status 
    {  
        no_timeout ,
        timeout
    } ;
    
    struct condition_variable final
    {
        using native_handle_type = CONDITION_VARIABLE * ;
        
        condition_variable () 
        { 
            InitializeConditionVariable( &cv_ ) ;
        }
        
        condition_variable ( condition_variable const & ) = delete ;
        condition_variable& operator = ( condition_variable const& ) = delete ;
        
        ~ condition_variable () { }
        
        void notify_one () noexcept 
        { 
            WakeConditionVariable( &cv_ ) ;
        }
        
        void notify_all () noexcept 
        { 
            WakeAllConditionVariable( &cv_ ) ;
        }
        
        void wait ( unique_lock< mutex >& lock )
        {
            /* multi(WinAPI)::mutex::native_handle_type is CRITICAL_SECTION * */
            if ( ! SleepConditionVariableCS( &cv_ , lock.mutex() -> native_handle() , INFINITE ) )    
                throw std::system_error{ ( int ) GetLastError() , std::system_category() } ; 
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
            return wait_for( lock , timeout_time - Clock::now() ) ;
        }
        
        template< class Clock, class Duration , class Predicate >
        bool wait_until( unique_lock< mutex >& lock,
                              const std::chrono::time_point< Clock, Duration >& timeout_time ,
                              Predicate pred )
        {
            return wait_for( lock , timeout_time - Clock::now()  , std::move( pred ) ) ;
        }
        
        template< class Rep, class Duration, class Predicate >
        cv_status wait_for( unique_lock< mutex >& lock,
                       const std::chrono::duration< Rep , Duration >& duration )
        {
            using namespace std::chrono ;
            
            if ( SleepConditionVariableCS( &cv_ , lock.mutex() -> native_handle() , duration_cast< milliseconds >( duration ).count() ) )
            {
                return cv_status::no_timeout ;
            }
            
            if ( GetLastError() == ERROR_TIMEOUT )
                return cv_status::timeout ; 
                
            throw std::system_error{ ( int ) GetLastError() , std::system_category() } ; 
        }
        
        template< class Rep, class Duration, class Predicate >
        bool wait_for( unique_lock< mutex >& lock,
                         const std::chrono::duration< Rep , Duration >& duration,
                         Predicate pred )
        {
            using namespace std::chrono ;
            while ( ! pred() ) 
            {
                if ( ! SleepConditionVariableCS( &cv_ , lock.mutex() -> native_handle() ,
                                                 duration_cast< milliseconds >( duration ).count() ) ) 
                {
                    if ( GetLastError() == ERROR_TIMEOUT )
                        return false ; 
                        
                    throw std::system_error{ ( int ) GetLastError() , std::system_category() } ; 
                }
            }
            return true ; 
        }       
        
        native_handle_type native_handle () noexcept /* not const */ 
        {  
            return &cv_ ;
        }
        
        private :    

            CONDITION_VARIABLE cv_ ;
    } ;
}

#endif // MULTI_THREAD_HXX
