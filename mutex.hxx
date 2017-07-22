#ifndef MULTI_MUTEX_HXX
#define MULTI_MUTEX_HXX

// GreenTree 2017 highcastle.cxx@gmail.com , MIT

#include <exception>
#include <system_error>
#include <memory>
#include <chrono>

#ifdef MULTI_POSIX_PLATFORM
    #include "sources/POSIX/mutex.hxx" 
#elif defined MULTI_WINAPI_PLATFORM
    #include "sources/WinAPI/mutex.hxx" 
#endif

namespace multi
{
    struct mutex final 
        : private p_::mutex::semantics 
    {
        using native_handle_type = p_::mutex::native_handle_type_ ;

        mutex () { impl_::init( &mtx_ ) ; } 
        ~ mutex () { impl_::destroy( &mtx_ ) ; }
        
        void lock ()   { impl_::lock( &mtx_ )   ; }
        void unlock () { impl_::unlock( &mtx_ ) ; }
        
        bool try_lock () noexcept { 
            return impl_::try_lock( &mtx_ ) ; 
        } 
        
        native_handle_type native_handle () noexcept { return impl_::handle( &mtx_ ) ; }
            
        private :
            using impl_ = p_::mutex ;
            impl_::underlying_t_ mtx_ ;
    } ;
    
    struct recursive_mutex final
        : private p_::recursive_mutex::semantics
    {
        using native_handle_type = p_::recursive_mutex::native_handle_type_ ;
        
        recursive_mutex () { impl_::init( &mtx_ ) ; } 
        ~ recursive_mutex () { impl_::destroy( &mtx_ ) ; }
        
        void lock ()     { impl_::lock( &mtx_ )   ; }
        void unlock ()   { impl_::unlock( &mtx_ ) ; }
        
        bool try_lock () noexcept { 
            return impl_::try_lock( &mtx_ ) ; 
        } 
        
        native_handle_type native_handle () noexcept { return impl_::handle( &mtx_ ) ; }
        private :
            using impl_ = p_::recursive_mutex ;
            impl_::underlying_t_ mtx_ ;
    } ;
    
    struct timed_mutex final 
        : private p_::timed_mutex::semantics
    {
        using native_handle_type = p_::timed_mutex::native_handle_type_ ;
        
        timed_mutex () { impl_::init( &mtx_ ) ; } 
        ~ timed_mutex () { impl_::destroy( &mtx_ ) ; }
        
        void lock ()   { impl_::lock( &mtx_ )   ; }
        void unlock () { impl_::unlock( &mtx_ ) ; }
        
        bool try_lock () noexcept { 
            return impl_::try_lock( &mtx_ ) ; 
        } 
        
        template < class Rep, class Period >
        bool try_lock_for( const std::chrono::duration< Rep , Period > & timeout_duration )
        {
            return impl_::try_lock_for( &mtx_ , timeout_duration ) ;
        }
        
        template< class Clock, class Duration >
        bool try_lock_until( const std::chrono::time_point< Clock , Duration >& timeout_time )
        {
            return impl_::try_lock_until( &mtx_ , timeout_time ) ;
        }
        
        native_handle_type native_handle () noexcept { return impl_::handle( &mtx_ ) ; }
        
        private :
            using impl_ = p_::timed_mutex ;
            impl_::underlying_t_ mtx_ ;
    } ;
    
    struct recursive_timed_mutex final
        : private p_::recursive_timed_mutex::semantics
    {
        using native_handle_type = p_::recursive_timed_mutex::native_handle_type_ ;
        
        recursive_timed_mutex () { impl_::init( &mtx_ ) ; }
        ~ recursive_timed_mutex () { impl_::destroy( &mtx_ ) ; }
        
        void lock ()   { impl_::lock( &mtx_ )   ; }
        void unlock () { impl_::unlock( &mtx_ ) ; }
        
        bool try_lock () noexcept { 
            return impl_::try_lock( &mtx_ ) ; 
        } 
        
        template < class Rep, class Period >
        bool try_lock_for ( const std::chrono::duration< Rep , Period > & timeout_duration ) 
        {
            return impl_::try_lock_for( &mtx_ , timeout_duration ) ;
        }
        
        template< class Clock, class Duration >
        bool try_lock_until( const std::chrono::time_point< Clock , Duration >& timeout_time )
        {
            return impl_::try_lock_until( &mtx_ , timeout_time ) ;
        }
        
        native_handle_type native_handle () noexcept { return impl_::handle( &mtx_ ) ; }
        
        private :
            using impl_ = p_::recursive_timed_mutex ;
            impl_::underlying_t_ mtx_ ;
    } ; 
    

    struct defer_lock_t  { } ;
    struct try_to_lock_t { } ;
    struct adopt_lock_t  { } ;
    
    constexpr defer_lock_t  defer_lock  ;
    constexpr try_to_lock_t try_to_lock ;
    constexpr adopt_lock_t  adopt_lock  ;
    
    template < class Lockable >
    struct lock_guard final 
    {
        using mutex_type = Lockable ;
                
        explicit lock_guard ( mutex_type& obj ) 
            : obj_ref_( obj ) 
        {
            obj_ref_.lock() ;
        }
        
        lock_guard ( mutex_type& obj , adopt_lock_t )  // the behaviour is undefined if current thread does not own the mutex already 
            : obj_ref_( obj )
        {
        }
        
        ~ lock_guard () 
        {
            obj_ref_.unlock() ;
        }
        
        private :
            mutex_type& obj_ref_ ;
    } ;
    

    template < class Lockable >
    struct unique_lock final // requires BasicLokable< Lockable >
    {
       using mutex_type = Lockable ;
       
       unique_lock () noexcept
            : obj_ptr_{} , is_locked_{}
       {
       }
       
       unique_lock ( unique_lock const& ) = delete ;
       
       unique_lock ( unique_lock&& src ) noexcept 
            : obj_ptr_{  src.obj_ptr_ } ,
              is_locked_{ src.is_locked_ }
       {
           src.obj_ptr_ = nullptr ;
           src.is_locked_ = false ;
       }
       
       unique_lock& operator = ( unique_lock const& ) = delete ;
       
       unique_lock& operator = ( unique_lock&& src ) noexcept
       { 
           if ( is_locked_ ) unlock() ;
           
           obj_ptr_ = src.obj_ptr_ ;
           src.obj_ptr_ = nullptr ;
           return * this ;
       }
       
       ~ unique_lock () 
       {
           // ( is_locked ) iff ( obj_ptr is valid )
           if ( is_locked_ )
               obj_ptr_ -> unlock () ;
       }
       
       explicit unique_lock ( mutex_type& obj ) 
            : obj_ptr_{ std::addressof( obj ) } 
       {
            obj_ptr_ -> lock() ;
            is_locked_ = true  ;
       }
       
       unique_lock ( mutex_type& obj , defer_lock_t ) noexcept 
            : obj_ptr_{ std::addressof( obj ) } , is_locked_{}
       {
       }
       
       unique_lock ( mutex_type& obj , try_to_lock_t )  
            : obj_ptr_{ std::addressof( obj ) }
       {
            obj_ptr_ -> try_lock() ;
            is_locked_ = true ;
       }
       
       unique_lock ( mutex_type& obj , adopt_lock_t ) 
            : obj_ptr_{ std::addressof( obj ) } , 
              is_locked_{ true }
       {
       }
       
       template< class Rep, class Period >
       unique_lock( mutex_type& obj , const std::chrono::duration< Rep , Period >& timeout_duration ) 
            : obj_ptr_{ std::addressof( obj ) }
       {
           is_locked_ = obj_ptr_ -> try_lock_for( timeout_duration ) ; ;
       }
    
       template< class Clock, class Duration >
       unique_lock( mutex_type& obj , const std::chrono::time_point< Clock , Duration >& timeout_time )
            : obj_ptr_{ std::addressof( obj ) }
       {
           is_locked_ = obj_ptr_ -> try_lock_until( timeout_time ) ;
       }
       
       void lock () 
       {
           lock_assert( obj_ptr_ , ! is_locked_ ) ;
           
           obj_ptr_ -> lock () ;
           is_locked_ = true ;
       }
       
       void unlock () 
       {
            if ( ! is_locked_ )
                throw std::system_error{ ( int ) std::errc::operation_not_permitted , 
                                          std::system_category() } ;
            
            obj_ptr_ -> unlock() ;
            is_locked_ = false ;
       }
       
       template < class Rep , class Period > // note : lazy instantiation
       bool try_lock_for ( const std::chrono::duration< Rep , Period > & timeout_duration ) 
       {
           lock_assert( obj_ptr_ , ! is_locked_ ) ;
           return is_locked_ = obj_ptr_ -> try_lock_for( timeout_duration ) ;
       }
        
       template< class Clock , class Duration > 
       bool try_lock_until( const std::chrono::time_point< Clock , Duration >& timeout_time )
       {   
           lock_assert( obj_ptr_ , ! is_locked_ ) ;
           return is_locked_ = obj_ptr_ -> try_lock_until( timeout_time ) ;
       }

       bool try_lock ()
       {   
           return is_locked_ = obj_ptr_ -> try_lock() ;
       }
       // modifiers :
       void swap ( unique_lock& obj ) noexcept
       {
           std::swap( obj_ptr_  , obj.obj_ptr_ ) ;
           std::swap( is_locked_ , obj.is_locked_ ) ;
       }
       
       mutex_type * release () const noexcept // no unlock is done
       { 
           mutex_type * tmp = obj_ptr_ ;
           obj_ptr_   = nullptr ;
           is_locked_ = false ;
           return tmp ;
       }
       
       // observers :
       bool owns_lock () const noexcept { return is_locked_ ; }
       explicit operator bool () const noexcept { return owns_lock () ; }
       mutex_type * mutex () noexcept { return obj_ptr_ ; }
       
       private :
           
            static void lock_assert ( bool ptr_state , bool locked_state )
            {
                if ( ! ptr_state )
                    throw std::system_error{ ( int ) std::errc::operation_not_permitted , 
                                             std::system_category() } ;
                if ( ! locked_state ) 
                    throw std::system_error{ ( int ) std::errc::resource_deadlock_would_occur , 
                                             std::system_category() } ;
            }
            
            mutex_type * obj_ptr_ ;
            bool is_locked_ ;
    } ;
}

#endif // MULTI_THREAD_HXX
