// g++ -Wall -pedantic -std=c++11 -D MULTI_POSIX_PLATFORM test0.cxx -lpthread
// i686-w64-mingw32-g++ -Wall -pedantic -static -std=c++11 -D MULTI_WINAPI_PLATFORM test0.cxx
// i686-w64-mingw32-g++ -Wall -pedantic -static -std=c++11 -D MULTI_POSIX_PLATFORM test0.cxx -lpthread
#include "../thread.hxx"
#include "../mutex.hxx"

#include <cassert>
#include <chrono>
#include <algorithm>
#include <iostream>

int main ( void ) 
{
    using namespace std::chrono ;
    namespace ns = multi ;
    
    using mutex_t = ns::recursive_timed_mutex ;
    
    mutex_t mtx ;
    
    ns::unique_lock< mutex_t > lock { mtx } ;
    
    ns::thread th0 , th1 ( [ &mtx ] () 
    {
        ns::unique_lock< mutex_t > lock { mtx , seconds{ 8 } } ;
        
        if ( lock ) {
            std::cerr << "boom" ;
            return ;
        }
     // */  
        std::cerr << "no boom" ;
    } ) ;
    
    assert( lock ) ;
    
    //std::this_thread::sleep_for( seconds{ 2 } ) ;
    lock.unlock() ;
    
    assert( ! lock ) ;

    assert( ! th0.joinable() ) ;
    assert( th1.joinable() ) ;
    
    th1.join() ;
}
