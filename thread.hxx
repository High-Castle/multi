#ifndef MULTI_THREAD_HXX
#define MULTI_THREAD_HXX

#ifdef MULTI_POSIX_PLATFORM
    #include "sources/POSIX/thread.hxx"
#elif defined MULTI_WINAPI_PLATFORM
    #include "sources/WinAPI/thread.hxx"
#endif 

#endif
