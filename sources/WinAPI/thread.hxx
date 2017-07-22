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
#include <windows.h> 
// i will just polute gl as that is done by most of implementations //



namespace multi
{                              
    namespace p_               
    {                          
        struct id_mirror ;     
    }   
    
    struct thread final
    {
        using native_handle_type = HANDLE ;
        
        
        // TODO : Instances of this class may also hold the special distinct value that does not represent any thread. 
        struct id final 
        {
            id () { } // TODO : constructs an id that does not represent a thread
            
            friend struct thread ;
            friend p_::id_mirror ;
            
            private :
                using underlying_t_ = DWORD ; // DWORD, as in 'DWORD WINAPI GetCurrentThreadId(void);'
                
                explicit id ( underlying_t_ id ) noexcept : id_( id ) 
                { 
                }
                
                underlying_t_ id_ ; 
            
            friend bool operator == ( id op0 , id op1 ) noexcept { return op0.id_ == op1.id_ ; }
            friend bool operator != ( id op0 , id op1 ) noexcept { return ! ( op0 == op1 ) ; }
            
            friend bool operator < ( id op0 , id op1 ) noexcept {
                return op0.id_ < op1.id_  ;
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
    
        thread  () : handle_{} { }
        
        thread ( const thread& ) = delete ;
        
        thread& operator = ( const thread& ) = delete ;
        
        thread  ( thread&& t ) noexcept
            : handle_( t.handle_ ) ,
              id_( t.id_ )
        {
             t.handle_ = nullptr ;
        }
        
        // If *this still has an associated running thread (i.e. joinable() == true), call std::terminate(). 
        // Otherwise, assigns the state of other to *this and sets other to a default constructed state.
        thread& operator = ( thread&& obj ) noexcept
        {
            if ( joinable() ) std::terminate() ;
            handle_ = obj.handle_ ;
            id_ = obj.id_ ;
            obj.handle_ = nullptr ;
            return * this ;
        }
        
        void swap( thread& obj ) noexcept
        {
            std::swap( obj.id_ , id_ ) ; 
            std::swap( obj.handle_ , handle_ ) ;
        }
        
        template < class F , class... Args >
        thread ( F&& func , Args&&... args )
        {
            auto * caller_wrapper_p = new auto( std::bind( std::forward< F >( func ) , 
                                                           std::forward< Args >( args )... ) ) ;
                                                           
            using caller_wrapper_t = typename std::remove_reference< decltype( * caller_wrapper_p ) >::type ;
            
            DWORD ( * caller )( LPVOID ) = [] ( LPVOID ptr_to_func ) -> DWORD 
            { 
                auto& caller_wrapper = * reinterpret_cast< caller_wrapper_t * >( ptr_to_func ) ;
                
                try { caller_wrapper() ; }
                catch ( ... ) 
                { 
                    delete ( caller_wrapper_t * ) ptr_to_func ; 
                    throw ; // terminate
                }
                delete ( caller_wrapper_t * ) ptr_to_func ;
                return 0 ;
            } ;
            
            id::underlying_t_ thread_id ;
            
            handle_ = CreateThread( nullptr , 0 , ( LPTHREAD_START_ROUTINE ) caller, caller_wrapper_p , 0 , &thread_id ) ;
            
            if ( ! handle_ ) 
              throw std::system_error{ ( int ) GetLastError() , std::system_category() } ;
            
            id_ = id( thread_id ) ;
        }
        
        ~ thread ()
        {
            if ( joinable () ) 
                std::terminate() ;
        }
        
        bool joinable () const noexcept { return handle_ != nullptr ; }
        id get_id () const noexcept { return id_ ; }
        native_handle_type native_handle() noexcept { return handle_ ; }
        
        void detach()  
        { 
            if ( ! joinable() ) 
                throw std::invalid_argument{ "nothing to detach()" } ;
            
            if ( ! CloseHandle( handle_ ) ) 
                throw std::system_error{ ( int ) GetLastError() , std::system_category() } ;
        
            handle_ = nullptr ;
        }
        
        void join ()
        {
            if ( ! joinable () ) 
                throw std::invalid_argument{ "nothing to join()" } ;
            
            DWORD waiting_result = WaitForSingleObject( handle_ , INFINITE ) ;
            
            if ( waiting_result == WAIT_FAILED ) 
                throw std::system_error{ ( int ) GetLastError() , std::system_category() } ;
            
            handle_ = nullptr ;
        }
    
        private :
            native_handle_type handle_ ;
            id id_ ;
    } ;
    
    namespace p_ {
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
            Sleep( duration_cast< milliseconds >( sleep_duration ).count() ) ;
        }
        
        template< class Clock, class Duration >
        void sleep_until( const std::chrono::duration< Clock, Duration >& time_point )
        {
            sleep_for( time_point - Clock::now() ) ;
        }
        
        inline void yield () noexcept
        {
            SwitchToThread() ;
        }
        
        inline thread::id get_id () 
        {
            return p_::id_mirror::create( GetCurrentThreadId() ) ;
        }
    } 
}

#endif // MULTI_THREAD_HXX
