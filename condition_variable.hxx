#ifdef MULTI_POSIX_PLATFORM
    #include "sources/POSIX/condition_variable.hxx"
#elif MULTI_WINAPI_PLATFORM
    #include "sources/WinAPI/condition_variable.hxx"
#endif
