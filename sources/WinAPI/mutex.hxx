#ifndef MULTI_MUTEX_IMPL_HXX
#define MULTI_MUTEX_IMPL_HXX

// GreenTree 2017 highcastle.cxx@gmail.com , MIT

#include <exception>
#include <system_error>
#include <type_traits>
#include <chrono>

#include <windows.h> 

namespace multi
{    
    namespace p_ 
    {
        struct static_class { static_class () = delete ; } ;
        
        struct mutex : static_class
        { 
            using underlying_t_ = CRITICAL_SECTION ;
            using native_handle_type_ = underlying_t_ * ;
            
            static native_handle_type_ handle ( underlying_t_ * obj_p ) 
            { 
                return obj_p ; 
            }
  
            static void init ( underlying_t_ * mtx_p )
            {
                /*__try { //
                    InitializeCriticalSection( mtx_p ) ;
                }
                __except( EXCEPTION_EXECUTE_HANDLER ) { // only one exception
                    throw std::system_error{ ( int ) std::not_enough_memory , std::system_category() } ;
                }*/
                try { 
                    InitializeCriticalSection( mtx_p ) ; 
                }
                catch ( ... ) { // only one exception
                    throw std::system_error{ ( int ) std::errc::not_enough_memory , std::system_category() } ;
                }
            }
            
            static void destroy ( underlying_t_ * mtx_p ) noexcept 
            { 
                DeleteCriticalSection( mtx_p ) ; 
            }
            
            static void lock ( underlying_t_ * mtx_p ) { // noexcept in windows.?
                while ( true ) {
                    try { 
                        EnterCriticalSection( mtx_p ) ;
                    }
                    catch( ... ) { // EXCEPTION_POSSIBLE_DEADLOCK; ( if waiting over time specified in registry; note that 
                                                            // documentation says do not ignore this exception ) (maybe log it somehow.?).
                        continue ;
                    }
                    break ;
                }
            }
                
            static void unlock ( underlying_t_ * mtx_p ) noexcept 
            {
                LeaveCriticalSection( mtx_p ) ;
            }
                
            static bool try_lock ( underlying_t_ * mtx_p ) noexcept 
            {
                return ( bool ) TryEnterCriticalSection( mtx_p ) ;
            }
                        
            struct semantics // non copiable, non movable
            {
                semantics() = default ;
                semantics( const semantics& ) = delete ; 
                semantics& operator = ( const semantics& ) = delete ;
                protected :
                    ~ semantics () = default ;
            } ;
        } ;
                

        
        struct timed_mutex : static_class
        {
            /* unfortunally there is no timed EnterCriticalSection ; 
             * implementing via Mutex */
            
            using underlying_t_ = HANDLE ;
            using native_handle_type_ = HANDLE ;
            
            static native_handle_type_ handle ( underlying_t_ * obj_p ) 
            { 
                return * obj_p ; 
            }
  
            static void init ( underlying_t_ * mtx_p )
            {
                * mtx_p = CreateMutex( nullptr , FALSE , nullptr ) ;
                if ( ! * mtx_p )
                    throw std::system_error{ ( int ) GetLastError() , std::system_category() } ;
            }
            
            static void destroy ( underlying_t_ * mtx_p ) noexcept 
            { 
                CloseHandle( * mtx_p ) ;
            }
            
            template< class Rep, class Period >
            static bool try_lock_for( underlying_t_ * mtx_p , std::chrono::duration< Rep , Period > const& duration )
            {
                using namespace std::chrono ;
                return wait_for_object( mtx_p , duration_cast< milliseconds >( duration ).count() ) ;
            } 
            
            template< class Clock , class Duration >
            static bool try_lock_until ( underlying_t_ * mtx_p , const std::chrono::time_point< Clock , Duration >& timeout_time )
            {
                using namespace std::chrono ;
                return try_lock_for( mtx_p , timeout_time - Clock::now() ) ;
            }
            
            static void lock ( underlying_t_ * mtx_p ) 
            { 
                wait_for_object( mtx_p , INFINITE ) ;
            }
                
            static void unlock ( underlying_t_ * mtx_p ) noexcept 
            { 
                ReleaseMutex( * mtx_p ) ;
            }
                
            static bool try_lock ( underlying_t_ * mtx_p ) noexcept 
            {
                return wait_for_object( mtx_p , 0 ) ;
            }
            
            struct semantics // non copiable, non movable
            {
                semantics() = default ;
                semantics( const semantics& ) = delete ; 
                semantics& operator = ( const semantics& ) = delete ;
                protected :
                    ~ semantics () = default ;
            } ;
            
            private :
                static bool wait_for_object( underlying_t_ * mtx_p , DWORD millisec ) 
                {
                    DWORD waiting_result = WaitForSingleObject( * mtx_p , millisec ) ;
                    
                    if ( waiting_result == WAIT_OBJECT_0 )
                        return true ;
                    
                    if ( waiting_result == WAIT_TIMEOUT )
                        return false ;
                   
                    // TODO 
                   // if ( waiting_result == WAIT_ABANDONED )
                     //   throw std::system_error{ ( int ) std:: , std::system_category() } ;
                        
                    throw std::system_error{ ( int ) GetLastError() , std::system_category() } ;
                }
        } ;
        

        struct recursive_mutex : mutex 
        { 
            /*
            After a thread has ownership of a critical section, it can make additional calls to EnterCriticalSection or TryEnterCriticalSection without blocking its execution. This prevents a thread from deadlocking itself while waiting for a critical section that it already owns. The thread enters the critical section each time EnterCriticalSection and TryEnterCriticalSection succeed. A thread must call LeaveCriticalSection once for each time that it entered the critical section.
            */
        } ;
        
        struct recursive_timed_mutex 
            : timed_mutex
        {
        } ;
    }
}

#endif // MULTI_THREAD_HXX
