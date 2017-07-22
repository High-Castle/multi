#ifndef MULTI_THREAD_POOL_HXX
#define MULTI_THREAD_POOL_HXX

// GreenTree 2017 highcastle.cxx@gmail.com , MIT

// http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_cond_signal.html
// if predictable scheduling behavior (.?) is required, then that mutex shall 
// be locked by the thread calling pthread_cond_broadcast() or pthread_cond_signal().

// predictible here probably relays directly to set scheduling mode, not to some posix stuff. 
// anyway for notify_all it's irrelevant, morover, most of posix implementation perform morphing.

// https://news.ycombinator.com/item?id=11893756
// http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/

#include <utility>
#include <bitset>
#include <iostream>
#include <ostream>
#include <cstddef>
#include <cassert>

#include "mutex.hxx"
#include "condition_variable.hxx"
#include "thread.hxx"


//using namespace std ;

namespace multi {

namespace p_ 
{
    template < class F >
    struct scope_guard final 
    {    
        explicit scope_guard( F func ) 
            : func_( std::move( func ) )
        {}
        
        ~ scope_guard () { 
           if ( ! discarded_ )
              try { func_() ; }
              catch ( ... ) { func_() ; }
        }  
        
        void perform () { 
            func_() ;
            discarded_ = true ;
        }
        
        void discard () noexcept { discarded_ = true ; }
    
    private :
        bool discarded_ = false ;
        F func_ ;
    } ;
    template < class F >
    inline scope_guard< F > make_guard ( F&& func ) {
        return scope_guard< F >{ std::forward< F >( func ) } ;
    }
}

struct RethrowThreadException // terminate
{
    void thread_exception_handle ( std::exception_ptr ptr )
    {
        std::cerr << "Exception in pool thread : " ; // note : not sync
        try { std::rethrow_exception( ptr ) ; }
        catch ( std::exception const& e ) {
            std::cerr << e.what () ; // note : not sync
            throw ;
        } catch ( ... ) {
            std::cerr << "Unrecognized exception" ; // note : not sync
            throw ;
        }
    }
    ~ RethrowThreadException () = default ;

} ;

struct TryLogThreadException 
{
    TryLogThreadException ()
    {
    }
    
    TryLogThreadException ( std::shared_ptr< mutex > mtx_ptr , 
                        std::shared_ptr< std::ostream > out_ptr )
        : mtx_ptr_( move( mtx_ptr ) ) , 
          out_ptr_( move( out_ptr ) )
        {   
        }
    
    void thread_exception_handle ( std::exception_ptr ptr ) noexcept
    {
        if ( ! mtx_ptr_ || ! out_ptr_ ) return ;
        
        try { mtx_ptr_ -> lock() ; }
        catch ( ... ) {}
        
        * out_ptr_ << "Exception in pool thread : " ;
        
        try { std::rethrow_exception( ptr ) ; }
        catch ( std::exception const& e ) {
            * out_ptr_ << e.what () ;
        } 
        catch ( ... ) {
            * out_ptr_ << "Unrecognized exception" ;
        }
        
        try { mtx_ptr_ -> unlock() ; }
        catch ( ... ) {}
    }
    ~ TryLogThreadException  () { } 
    private :
        
        std::shared_ptr< mutex > mtx_ptr_ ;
        std::shared_ptr< std::ostream > out_ptr_ ;
} ;

template 
<
    class Queue ,
    class ThreadExceptionPolicy = RethrowThreadException 
>
struct thread_pool final 
    : private ThreadExceptionPolicy
{
    using task_type = typename Queue::value_type ;
    using task_queue_type = Queue ;
    using exception_policy_type = ThreadExceptionPolicy ;
    
    explicit thread_pool( std::size_t const thread_num = 0 , 
                          ThreadExceptionPolicy policy = ThreadExceptionPolicy() ) 
        : ThreadExceptionPolicy( std::move( policy ) ) ,
          thread_count_{} , active_count_{} ,
          state_{ thread_num ? EXECUTING : PAUSED } 
    {
      // if ( auto  = add_thread( thread_num ) ;
         add_thread( thread_num ) ;
    }
    
    ~ thread_pool ()
    {
        try { clear() ; } // it has not to be concurrent call
        catch ( ... ) { /* log */ }
    }
    
    thread_pool( const thread_pool & ) = delete ;
    thread_pool& operator = ( const thread_pool & ) = delete ;
            
    // number that was created till exception
    void add_thread ( std::size_t const thread_num = 1 , bool resume_if_paused = true ) // TODO
    {
        lock_guard< mutex > op_lock { op_mtx_ } ;
        unique_lock< mutex > lock { queue_mtx_ } ;
        
        std::size_t const new_count = thread_count_ + thread_num ;
        
        for ( std::size_t count = 0 ; count < thread_num ; ++ count ) 
        {
            try { thread( &thread_pool::routine , this ).detach() ; }
            catch ( ... )
            {
                //action_ = FINISH ;
                //queue_cv_.notify_all() ;        
                throw ;
            }
        }
        //if (  )
        
        client_cv_.wait( lock , [ this , new_count ] { return thread_count_ == new_count ; } ) ;
    }
    
    void remove_thread ()                                                               // TODO
    {
        lock_guard< mutex > op_lock { op_mtx_ } ;
        unique_lock< mutex > lock { queue_mtx_ } ;
        
        if ( thread_count_ == 0 ) 
            throw std::logic_error{ "attempt to remove non-existing thread" } ;
        
        std::size_t const new_count = thread_count_ - 1 ;
        
        auto const prev_state = state_ ;
        state_ = PAUSED ;
        
        auto set_state_back 
            = p_::make_guard( [ this , prev_state , new_count ] () { 
                if ( new_count != 0 ) {
                    state_ = prev_state ;
                    queue_cv_.notify_all( ) ;
                }
            } ) ;
        
        client_cv_.wait ( lock , [ this ] ( ) { // note : may throw
            return active_count_ != thread_count_ ; 
        } ) ;
        
        action_[ FINISH ] = true ;
        
        queue_cv_.notify_one() ;
        
        client_cv_.wait( lock , [ this , new_count ] ( ) { // note : may throw
            return thread_count_ == new_count ; 
        } ) ;
    }
    
    void clear ()
    {
        lock_guard< mutex > op_lock { op_mtx_ } ;
        unique_lock< mutex > lock { queue_mtx_ } ;
        
        if ( ! thread_count_ ) 
            throw std::logic_error{ "attempt to remove non-existing thread" } ;
        
        state_ = PAUSED ;
        
        client_cv_.wait ( lock , [ this ] ( ) { 
            return active_count_ == 0 ; 
        } ) ;
        
        action_[ FINISH ]     = true ; 
        action_[ FINISH_ALL ] = true ;

        queue_cv_.notify_all() ;
        
        client_cv_.wait ( lock , [ this ] ( ) { // note : may throw
            return thread_count_ == 0 ; 
        } ) ;
    }
    
    void join () // is needed to ensure that all tasks from current thread are done. 
                // it's a general, obviously not excelent solution. to avoid this join, 
                // task type with future support can be used ( or just with some discarding mechanism )
    {
        unique_lock< mutex > lock { queue_mtx_ } ;
        client_cv_.wait( lock , 
                   [ this ] ( ) { return queue_.empty() && active_count_ == 0 ; } ) ; // active_count_ == 0 ;
        
    }
        
    void discard_queue () 
    { 
        lock_guard< mutex > lock { queue_mtx_ } ;
        queue_.clear() ; 
    }
    
    void enqueue ( task_type the_task ) 
    {
        unique_lock< mutex > lock { queue_mtx_ } ;
        queue_.emplace( std::move( the_task ) ) ;
        queue_cv_.notify_one() ; // vs chained notify_one whith mutex unlocked (prof).?
    }
    
    template < class InputIt >
    void enqueue ( InputIt& it , InputIt to ) // move iterator
    {
        lock_guard< mutex > lock { queue_mtx_ } ;
        
        if ( it == to ) 
            return ;
        
        for ( std::size_t count = 0 ; it != to ; ++ it ) try 
        {
            queue_.emplace( * it ) ;
            ++ count ;
        } 
        catch ( ... ) {
            if ( count ) {
                queue_cv_.notify_all() ;
            }
            throw ; 
        }
        queue_cv_.notify_all() ;
    }
    
    
    bool pause () 
    {
        lock_guard< mutex > lock { queue_mtx_ } ;
        if ( state_ == PAUSED ) return false ;
        state_ = PAUSED ; 
        return true ;
    }
    
    bool resume () 
    {
        lock_guard< mutex > lock { queue_mtx_ } ;
        
        if ( state_ == EXECUTING ) {
            assert( thread_count_ != 0 ) ;
            return false ;
        }
        
        if ( thread_count_ == 0 ) 
            throw std::logic_error{ "there is no thread to resume" } ;
        
        state_ = EXECUTING ;
        queue_cv_.notify_all() ;
        return true ;
    }
    
    exception_policy_type& exception_policy() {
        return * reinterpret_cast< ThreadExceptionPolicy * >( this ) ;
    }
    
    private :
        
        void routine () try
        {
            unique_lock< mutex > lock { queue_mtx_ } ;
            
            ++ thread_count_ ;
            bool is_active ;
            
            auto on_thread_exit = 
                p_::make_guard( [ this , &is_active , &lock ] () {
                    assert( lock ) ;
                    -- thread_count_ ;
                    if ( is_active )
                        -- active_count_ ;
                    client_cv_.notify_all() ;
                } ) ;
            
            for ( ; ; )
            {
            
                is_active = false ;
                if ( active_count_ ) 
                    -- active_count_ ;
                
                client_cv_.notify_all() ;
                
                queue_cv_.wait( lock , [ this ] ( ) { return action_.any() 
                                                            || ( state_ != PAUSED && ! queue_.empty() ) ; } ) ;
                
                if ( action_[ FINISH ] ) 
                {
                    // assert( op_mtx_ is locked by prod thread ) ;
                    if ( ! action_[ FINISH_ALL ] ) {
                        action_[ FINISH ] = false ;
                        break ;
                    }
                    
                    if ( thread_count_ == 1 ) // last one
                        action_[ FINISH_ALL ] = false ;
                    break ;
                }
                    
                task_type task = std::move( queue_.front() ) ;
                queue_.pop() ;
                
                is_active = true ;
                ++ active_count_ ;
                
                client_cv_.notify_all() ;
                
                
                lock.unlock () ;
                
                auto lock_again_at_the_end = 
                    p_::make_guard( [ this , &lock ] () { lock.lock() ; } ) ;

                try { task() ; }
                catch ( ... ) { /* todo ; may throw in future */ }
            } 
        } catch ( ... ) 
          {
              exception_policy_type::thread_exception_handle( std::current_exception() ) ;
          }
        
        enum EThreadAction
        {
           FINISH , //
           FINISH_ALL ,
        EThreadAction_SZ
        } ;
        
        enum EPoolState
        {
           PAUSED ,
           EXECUTING 
        } ;
        

        mutex queue_mtx_ , op_mtx_ ; // op_mtx_ is used for operations that are waiting for rresponse ; can be locked only by thread_pool
                                     // must be locked strictly before queue_mtx_ 
        condition_variable queue_cv_ ,  // queue_mtx_ ; can be waited only within routine. 
                           client_cv_ ; // queue_mtx_ ; waiters,  etc
        
        task_queue_type queue_ ;
        
        std::size_t thread_count_   ; // queue_mtx_                         || increased / decremented by working threads
        std::size_t active_count_   ; // queue_mtx_ + client_cv_.notify_all || increased / decremented by working threads
        
        EPoolState state_  ;
        std::bitset< EThreadAction_SZ > action_ ;
        
} ; // thread_pool

} // multi



/*
struct EventScheduler
{

   for ( each : client_groups )
       pool.enqueue( notify_client_group , group , message ) ;
   
   std::unourdered_map< multi::thread::id 
} ;


struct notify_counter final
{
    using value_type = unsigned long long ;
    
    notify_counter& operator ++ () noexcept { ++ count_ ; return * this ; }
    
    private :
        value_type count_ = 0 ; // 
    
    friend bool operator == ( notify_counter const& op0 ,  
                              notify_counter const& op1 )
    {
        return op0.count_ == op1.count_ ;
    }
    
    friend bool operator != ( notify_counter const& op0 ,  
                              notify_counter const& op1 )
    {
        return op0.count_ != op1.count_ ;
    }
    
    friend value_type operator - ( notify_counter const& recent , 
                                   notify_counter const& later ) {
        if ( recent.count_ < later.count_ ) {
            return std::numeric_limits< value_type >::max - later.count_ + recent.count_ ;
        }
        return recent.count_ - later.count_ ;
    }
} ;

template < class T >
struct future final
{
    template < class Rep , class  >
    bool wait_until( counter_integer_t count , std::chrono::time_point< Rep ,  > )
    {
        
    }
    template < class Rep , class  >
    bool wait_for( counter_integer_t count , std::chrono::duration< Rep ,  > )
    {
        
    }
    
    
    
    private :
} ;

template < class T >
struct promise final
{
    private :
} ;

struct counter final
{
    using counter_integer_t = unsigned long long ;

    
    
    

    private :
 
} ;

template < class >
struct task final
{
    template< class F , class... Args >
    task( F&& f , Args&&... args )
    {
        task_ = std::bind( std::forward< F >( f ) ,
                           std::forward< Args >( args )... )
    }
    
    void execute() noexcept 
    {
        multi::unique_lock
        try { task_() ; } 
        catch( ... ) {
            ptr_ = std::current_exception() ;
        }
        cv_.notify_all() ;
    }
    
    private :
        multi::mutex mtx_ ;
        multi::condition_variable cv_ ;
        std::exception_ptr ptr_ ;
        std::function< void() > task_ ;
} ;


*/
#endif 
