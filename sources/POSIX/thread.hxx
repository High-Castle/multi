#ifndef MULTI_THREAD_IMPL_HXX
#define MULTI_THREAD_IMPL_HXX

// GreenTree 2017 highcastle.cxx@gmail.com , MIT

#include <exception>
#include <system_error>
#include <type_traits>
#include <functional>
#include <utility>

// will be isolated somehow (excluded for all other files except this)                  //
// do not know how to do this for now, probably with some compiler-specific features    //
#include <pthread.h> 
#include <unistd.h>
// i will just polute gl as that is done by most of implementations //



namespace multi
{    
                                
    namespace p_              
    {                          
        struct id_mirror ;    
    }                          
    
    struct thread final
    {
        using native_handle_type = pthread_t ;
        
        
        // TODO : Instances of this class may also hold the special distinct value that does not represent any thread. 
        struct id final 
        {
            id () { } // TODO : constructs an id that does not represent a thread
            
            friend struct thread ;
            friend p_::id_mirror ;
            
            private :
                using underlying_t_ = native_handle_type ;
                
                id ( underlying_t_ id ) 
                    : id_( id ) 
                { 
                    static_assert( std::is_arithmetic< native_handle_type >::value , 
                                   "pthread_t must be an arithmetical type" ) ;
                }
                underlying_t_ id_ ;
            
            friend bool operator == ( id op0 , id op1 ) noexcept { return pthread_equal( op0.id_ , op1.id_ ) ; }
            friend bool operator != ( id op0 , id op1 ) noexcept { return ! ( op0 == op1 ) ; }
            
            friend bool operator < ( id op0 , id op1 ) noexcept {
                return op0.id_ < op1.id_  ; // POSIX do not require pthread_t to be arithmetical; 
                                            // so it is only an assumption for this pthread implementation
            }
            friend bool operator >  ( id op0 , id op1 ) noexcept { return op0.id_ > op1.id_  ; }
            friend bool operator <= ( id op0 , id op1 ) noexcept { return op0.id_ <= op1.id_ ; }
            friend bool operator >= ( id op0 , id op1 ) noexcept { return op0.id_ >= op1.id_ ; }
            
            template< class CharT , class Traits >
            friend std::basic_ostream< CharT , Traits >& 
                operator << ( std::basic_ostream< CharT , Traits >& ost , id id )  
            {
                ost << id.id_ ;
                return ost ;
            }
        } ;
    
        thread  () : is_joinable_( false ) { }
        
        thread ( const thread& ) = delete ;
        
        thread& operator = ( const thread& ) = delete ;
        
        thread  ( thread&& t ) noexcept
            : thread_id_( t.thread_id_ ) ,
              is_joinable_( t.is_joinable_ )
        {
            t.is_joinable_ = false ;
        }
        
        // If *this still has an associated running thread (i.e. joinable() == true), call std::terminate(). 
        // Otherwise, assigns the state of other to *this and sets other to a default constructed state.
        thread& operator = ( thread&& obj ) noexcept
        {
            if ( joinable() ) std::terminate() ;
            is_joinable_ = obj.is_joinable_ ;
            thread_id_ = obj.thread_id_ ;
            obj.is_joinable_ = false ;
            return * this ;
        }
        
        void swap( thread& obj ) noexcept
        {
            std::swap( obj.is_joinable_ , is_joinable_ ) ; 
            std::swap( obj.thread_id_ , thread_id_ ) ;
        }
        
        template < class F , class... Args >
        thread ( F&& func , Args&&... args )
        {
            auto * caller_wrapper_p = new auto( std::bind( std::forward< F >( func ) , 
                                                           std::forward< Args >( args )... ) ) ;
                                                           
            using caller_wrapper_t = typename std::remove_reference< decltype( * caller_wrapper_p ) >::type ;
            
            auto caller = [] ( void * ptr_to_func ) -> void *
            { 
                auto& caller_wrapper = * reinterpret_cast< caller_wrapper_t * >( ptr_to_func ) ;
                
                try { caller_wrapper() ; }
                catch ( ... ) 
                { 
                    delete ( caller_wrapper_t * ) ptr_to_func ; 
                    throw ; // terminate
                }
                delete ( caller_wrapper_t * ) ptr_to_func ;
                return nullptr ;
            } ;
            
            if ( int error = pthread_create( &thread_id_ , nullptr , caller , caller_wrapper_p ) ) 
              throw std::system_error{ error , std::system_category() } ;
            
            is_joinable_ = true ;
        }
        
        ~ thread ()
        {
            if ( joinable () ) 
                std::terminate() ;
        }
        
        bool joinable () const noexcept { return is_joinable_ ; }
        id get_id () const noexcept { return id( thread_id_ ) ; }
        native_handle_type native_handle() noexcept { return thread_id_ ; }
        
        void detach()  
        { 
            if ( ! joinable() ) 
                throw std::invalid_argument{ "nothing to detach()" } ;
            
            if ( int error = pthread_detach( thread_id_ ) ) 
                throw std::system_error{ error , std::system_category() } ;
        
            is_joinable_ = false ;
        }
        
        void join ()
        {
            if ( ! joinable () ) 
                throw std::invalid_argument{ "nothing to join()" } ;
            
            if ( int error = pthread_join( thread_id_ , nullptr ) ) 
                throw std::system_error{ error , std::system_category() } ;
            
            is_joinable_ = false ;
        }
    
        private :
            native_handle_type thread_id_ ;
            bool is_joinable_ ;
    } ;
    
    namespace p_
    {
        struct id_mirror final {
            static thread::id create( thread::id::underlying_t_ id_handle_ ) 
            {
                return thread::id( id_handle_ ) ;
            }
        } ;
    }
    
    namespace this_thread
    {
        template< class Rep, class Period >
        void sleep_for( const std::chrono::duration<Rep, Period>& sleep_duration )
        {
            using namespace std::chrono ;
            usleep( duration_cast< microseconds >( sleep_duration ).count() ) ;
        }
        
        template< class Clock, class Duration >
        void sleep_until( const std::chrono::duration< Clock, Duration >& time_point )
        {
            sleep_for( time_point - Clock::now() ) ;
        }
        
        inline thread::id get_id () 
        {
            return p_::id_mirror::create( pthread_self() ) ;
        }
        
        inline void yield () noexcept
        {
            sched_yield () ; 
        }
    } 
}

#endif // MULTI_THREAD_HXX
