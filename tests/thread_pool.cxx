#include <iostream>
#include <queue>
#include <functional>
#include <vector>

#include "../thread_pool.hxx"
#include "../mutex.hxx"
#include "../thread.hxx"

struct priorited_function final
{
    using priority_type = unsigned ;
    
    priorited_function( priority_type prio , 
                        std::function< void() > func ) 
        : func_{ std::move( func ) } , prio_{ prio }
    {
    }
    
    void operator () () { func_() ; }
    priority_type priority () const noexcept { return prio_ ; }
    
    private :
       std::function< void() > func_ ; 
       priority_type prio_ ;
} ;

bool operator < ( priorited_function const& func0 , priorited_function const& func1 )
{
    return func0.priority() < func1.priority() ;
}

template < class... Args >
struct pqueue final
    : private std::priority_queue< Args... > // tmp
    {
        using std::priority_queue< Args... >::priority_queue ;
        using std::priority_queue< Args... >::pop ;
        using std::priority_queue< Args... >::emplace ;
        using std::priority_queue< Args... >::empty ;
        using typename std::priority_queue< Args... >::value_type ;
        void clear () { this -> c.clear() ; }
        value_type const& front () const noexcept { return this -> top() ; }
    } ;

int main ()
{
    using namespace std::chrono ;
    using function_queue = std::queue< std::function< void() > > ;
    using priority_funcqueue = pqueue< priorited_function > ;
    
    multi::thread_pool< priority_funcqueue > pool{ 4 } ;
    
    multi::mutex cerr_mtx ;

    std::size_t const times = 100000 ;

    std::size_t count_times = 0 ;
    auto func = [ & ] ( std::size_t call_no ) 
    {    
        { multi::lock_guard< multi::mutex > lock { cerr_mtx } ;
          ++ count_times ;
          std::cerr << "\n priority : " << call_no << " : hi from thread " << multi::this_thread::get_id() ; }
        
        multi::this_thread::sleep_for( milliseconds{ 1 } ) ;
    } ;
    
    auto enqueuer = [ & ] () { 
        for ( std::size_t count = 0 ; count < times ; ++ count )
            pool.enqueue( priorited_function( count % 6 , std::bind( func , count % 6 ) ) ) ; 
        
        multi::lock_guard< multi::mutex > lock { cerr_mtx } ;
        std::cerr << "\n\nenqueuer done.!\n" ;
        multi::this_thread::sleep_for( seconds{ 1 } ) ;
    } ;
    
    auto discarder = [ & ] ( ) { 
        for ( std::size_t count = 0 ; count < 3 ; ++ count ) {
            multi::this_thread::sleep_for( seconds{ 6 } ) ;
            pool.discard_queue() ;
            
            multi::lock_guard< multi::mutex > lock { cerr_mtx } ;
            std::cerr << "\n\ndiscarded.!\n" ;
            multi::this_thread::sleep_for( seconds{ 1 } ) ;
        }
    } ;
    
    std::vector< multi::thread > another_workers ;
    another_workers.emplace_back( enqueuer ) ;
    another_workers.emplace_back( enqueuer ) ;
    another_workers.emplace_back( enqueuer ) ;
    another_workers.emplace_back( discarder ) ;
    
    multi::thread pause( [ & ] ( ) { 
        multi::this_thread::sleep_for( seconds{ 3 } ) ;
        pool.pause() ;
        multi::lock_guard< multi::mutex > lock { cerr_mtx } ;
        std::cerr << "\npaused.!\n" ;
        multi::this_thread::sleep_for( seconds{ 1 } ) ;
    } ) ; 
    
    multi::thread resume( [ & ] ( ) { 
        multi::this_thread::sleep_for( seconds{ 10 } ) ;
        pool.resume() ;
        multi::lock_guard< multi::mutex > lock { cerr_mtx } ;
        std::cerr << "\nresumed.!\n" ;
        multi::this_thread::sleep_for( seconds{ 1 } ) ;
    } ) ;
    
    for ( auto& each : another_workers )
        each.join() ;
    
    pool.join() ;
    
    std::cerr << "\ncounted : " << count_times ;

    pause.join() , resume.join() ;
    
    std::cerr << "\nbue" ;
}
