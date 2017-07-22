#ifndef MULTI_MUTEX_IMPL_HXX
#define MULTI_MUTEX_IMPL_HXX

// GreenTree 2017 highcastle.cxx@gmail.com , MIT

#include <exception>
#include <system_error>
#include <type_traits>
#include <chrono>

#include <pthread.h> 
#include <time.h>

namespace multi
{    
    namespace p_ 
    {
        struct mutex
        {
            mutex () = delete ;
            
            using underlying_t_ = pthread_mutex_t ;
            using native_handle_type_ = pthread_mutex_t * ;
            
            static native_handle_type_ handle ( underlying_t_ * obj_p ) { return obj_p ; }
  
            // PTHREAD_MUTEX_NORMAL  - thread attempting to relock this mutex without first unlocking it will deadlock. 
            
            // PTHREAD_MUTEX_DEFAULT - Attempting to recursively lock a mutex of this type results in undefined behaviour.
            //                         ( as stated for std::mutex )
    
            static void init ( underlying_t_ * mtx_p )
            {
                mtx_type_init( mtx_p , PTHREAD_MUTEX_DEFAULT ) ;
            }
            
            static void destroy ( underlying_t_ * mtx_p ) noexcept { pthread_mutex_destroy( mtx_p ) ; }
            
            static void lock ( underlying_t_ * mtx_p ) { 
                if ( int err = pthread_mutex_lock( mtx_p ) )
                    throw std::system_error{ err , std::system_category() } ;
            }
                
            static void unlock ( underlying_t_ * mtx_p ) noexcept {
                pthread_mutex_unlock( mtx_p ) ;
            }
                
            static bool try_lock ( underlying_t_ * mtx_p ) noexcept
            {
                return ! pthread_mutex_trylock( mtx_p ) ;
            }
                        
            struct semantics // non copiable, non movable
            {
                semantics() = default ;
                semantics( const semantics& ) = delete ; 
                semantics& operator = ( const semantics& ) = delete ;
                protected :
                    ~ semantics () = default ;
            } ;
            protected :
                static void mtx_type_init ( underlying_t_ * mtx_p , int type ) 
                {
                    // http://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread_mutexattr_settype.html
                    pthread_mutexattr_t attr ; 

                    if ( int err = pthread_mutexattr_init( &attr ) )
                        throw std::system_error{ err , std::system_category() } ;
                    
                    if ( int err = pthread_mutexattr_settype( &attr , type ) ) {
                        pthread_mutexattr_destroy( &attr ) ;
                        throw std::system_error{ err , std::system_category() } ;
                    }
                    
                    if ( int err = pthread_mutex_init( mtx_p , &attr ) ) {
                        pthread_mutexattr_destroy( &attr ) ;
                        throw std::system_error{ err , std::system_category() } ;
                    }
                    
                    pthread_mutexattr_destroy( &attr ) ;
                }
        } ;
        
        /* http://en.cppreference.com/w/cpp/thread/condition_variable/wait_until
         * The clock tied to timeout_time is used, which is not required to be a monotonic clock.
         * There are no guarantees regarding the behavior of this function if the clock is adjusted 
         * discontinuously, but the existing implementations convert timeout_time from Clock to
         * std::chrono::system_clock and delegate to POSIX pthread_cond_timedwait ( timespec ) so that 
         * the wait honors ajustments to the system clock, but not to the the user-provided Clock. In any case, 
         * the function also may wait for longer than until after timeout_time has been reached 
         * due to scheduling or resource contention delays. 
         */
        
        // TODO 
        struct timed_mutex : mutex 
        {
            template< class Clock , class Duration >
            static bool try_lock_until ( underlying_t_ * mtx_p , const std::chrono::time_point< Clock , Duration >& timeout_time )
            {
                using namespace std::chrono ;
                
                auto sys_time_point = std::is_same< Clock , system_clock >::value ? timeout_time
                                            : system_clock::now() + ( timeout_time - Clock::now() ) ;
                
                auto duration_since_epoch = sys_time_point.time_since_epoch() ;
                
                time_t sec = duration_cast< seconds >( duration_since_epoch ).count() ;
                long nsec = duration_cast< nanoseconds >( duration_since_epoch 
                                           - duration_cast< seconds >( duration_since_epoch ) ).count() ;
                
                timespec wait_until = { sec , nsec } ;
                return ! pthread_mutex_timedlock( mtx_p , &wait_until ) ;
            }
            

            
            template< class Rep, class Period >
            static bool try_lock_for( underlying_t_ * mtx , std::chrono::duration< Rep , Period > const& timeout_duration )
            {
                using namespace std::chrono ;
                return try_lock_until( mtx , system_clock::now() + timeout_duration ) ;
            } 
        } ;
        
        struct recursive_mutex : mutex 
        { 
            static void init ( underlying_t_ * mtx_p ) 
            {
                mtx_type_init( mtx_p , PTHREAD_MUTEX_RECURSIVE ) ;
            }
        } ;
        
        struct recursive_timed_mutex 
            : recursive_mutex , timed_mutex
        {
            using recursive_mutex::init ;
        } ;
    }
    
    
    

    // TODO : std::lock( lk0 , lk1 , ... ) 
}

#endif // MULTI_THREAD_HXX
