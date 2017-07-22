#include <iostream>
#include <chrono>

#include "../condition_variable.hxx"
#include "../thread.hxx"
#include "../mutex.hxx"

int main() 
{
    using namespace std::chrono ;
    namespace ns = multi ;

	bool cond = false ;
	ns::condition_variable cv ;
	ns::mutex mtx ;
    
    
    auto func = [ &cond , &cv , &mtx ] 
    { 
        { 
          ns::unique_lock< ns::mutex > lock { mtx } ;
          if ( ! cv.wait_for( lock , seconds{ 2 } + nanoseconds{ 6 } , [ &cond ] () { return cond ; } ) )
          {
              std::cerr << "timeout.!" ;
              return ; 
          }
        }
 
		std::cerr << "hi" ; // concurent
	} ;
	
    
    ns::thread th0 { func } , th1 { func } ;
    
    cv.notify_all() ;
    
    std::cerr << "making the condition\n" ;
    
    ns::this_thread::sleep_for( seconds{ 2 } ) ;
    
    { ns::unique_lock< ns::mutex > lock { mtx } ;
      cond = true ; }
    
    cv.notify_all() ;
    
    th0.join() ;
    th1.join() ;
	return 0;
}
