
/* vim: set et ts=4 sw=4 ft=cpp:
 *
 * Copyright (C) 2011, 2012 James McLaughlin.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LacewingCommon
#define LacewingCommon

#define LacewingInternal

#ifdef _MSC_VER
    #pragma warning (disable : 4355) /* 'this' : used in base member initializer list */
#endif

#if defined(_WIN32) && !defined (LacewingWindows)
    #define LacewingWindows
#endif

#ifdef LacewingWindows

    #ifdef _DEBUG
        #define LacewingDebug
    #endif

    #ifdef _WIN64
        #define Lacewing64
    #endif

    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif

    #ifndef _CRT_NONSTDC_NO_WARNINGS
        #define _CRT_NONSTDC_NO_WARNINGS
    #endif

    #ifdef HAVE_CONFIG_H
        #include "../config.h"
    #endif

#else

    #ifdef ANDROID
        #define LacewingAndroid
        #include "../jni/config.h"
    #else
        #ifdef HAVE_CONFIG_H
            #include "../config.h"
        #else
            #error Valid config.h required for non-Windows! Run ./configure
        #endif
    #endif

#endif

#ifdef LacewingLibrary
    #ifdef LacewingWindows
        #define LacewingFunction __declspec(dllexport)
    #else
        #ifdef __GNUC__
            #define LacewingFunction __attribute__((visibility("default")))
        #else
            #error "Don't know how to build the library with this compiler"
        #endif
    #endif
#else
    #define LacewingFunction
#endif

inline void LacewingAssert(bool Expression)
{
#ifdef LacewingDebug

	if(Expression)
		return;

	#ifdef _MSC_VER
		__asm int 3;
	#else
		(*(int *) 0) = 1;
	#endif	
	
#endif
}

void LacewingInitialise();

#include "../include/Lacewing.h"
using namespace Lacewing;

#ifdef LacewingSPDY
    #include "../deps/zlib/zlib.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <new>

const int lw_max_path = 512;

#ifdef LacewingWindows

    #ifndef WINVER
        #define WINVER 0x0501
    #endif

    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0501
    #endif

    #define I64Format   "%I64d"
    #define UDFormat    "%d"

    #include <winsock2.h>
    #include <windows.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>

    #define SECURITY_WIN32

    #include <security.h>
    #include <sspi.h>
    #include <wincrypt.h>
    #include <schannel.h>
    #include <process.h>
    #include <ctime>

    #undef SendMessage
    #undef Yield

    #include "windows/Compat.h"

    #define strcasecmp stricmp

    typedef SOCKET lw_socket;

    #define LacewingGetSocketError() WSAGetLastError()
    #define LacewingGetLastError() GetLastError()

    /* Safe providing gmtime() is never used outside this function */

    extern Lacewing::Sync Sync_GMTime;

    inline void gmtime_r(const time_t * T, tm * TM)
    {
        Lacewing::Sync::Lock Lock(Sync_GMTime);

        tm * _TM = gmtime(T);

        if(!_TM)
        {
            memset(TM, 0, sizeof (tm));
            return;
        }

        memcpy(TM, _TM, sizeof (tm));
    }

    #define LacewingYield()             Sleep(0)

    #define lw_vsnprintf                _vsnprintf
    #define lw_snprintf                 _snprintf

    namespace Lacewing
    {
        inline time_t FileTimeToUnixTime(const FILETIME &FileTime)
        {
            LARGE_INTEGER LargeInteger;

            LargeInteger.LowPart = FileTime.dwLowDateTime;
            LargeInteger.HighPart = FileTime.dwHighDateTime;

            return (time_t) ((LargeInteger.QuadPart - 116444736000000000) / 10000000);
        }
            
        inline void LacewingCloseSocket (lw_socket Socket)
        {
            if (Socket == -1)
                return;

            CancelIo ((HANDLE) Socket);
            closesocket (Socket);
        }
    }

#else

    #ifndef Lacewing64
        #define I64Format   "%lld"
    #else
        #define I64Format   "%ld"
    #endif
    
    #define UDFormat    "%ud"

    #include <sys/types.h> 
    #include <sys/stat.h>
    #include <sys/socket.h>
    #include <sys/poll.h>
    #include <sys/utsname.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <pthread.h>
    #include <errno.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <sched.h>
    
    #ifdef LacewingAndroid
        #include <time64.h>
        #include <android/log.h>
    #endif
    
    #ifdef HAVE_SYS_TIMERFD_H
        #include <sys/timerfd.h>
        
        #ifndef LacewingUseKQueue
            #define LacewingUseTimerFD
        #endif
    #endif
    
    #ifdef HAVE_SYS_SENDFILE_H
        #include <sys/sendfile.h>
    #endif

    #ifdef HAVE_NETDB_H
        #include <netdb.h>
    #endif

    #if HAVE_DECL_TCP_CORK
        #define LacewingCork TCP_CORK
    #else
        #if HAVE_DECL_TCP_NOPUSH
            #define LacewingCork TCP_NOPUSH 
        #else
            #error "No TCP_CORK or TCP_NOPUSH available on this platform"
        #endif
    #endif

    #ifndef __APPLE__
        #define LacewingAllowCork
    #endif

    #ifdef HAVE_SYS_EPOLL_H

        #define LacewingUseEPoll
        #include <sys/epoll.h>

        #if !HAVE_DECL_EPOLLRDHUP
            #define EPOLLRDHUP 0x2000
        #endif

    #else
        #ifdef HAVE_KQUEUE
            #define LacewingUseKQueue
            #include <sys/event.h>
        #else
            #error Either EPoll or KQueue required to build Lacewing
        #endif
    #endif

    #include <string.h>
    #include <limits.h>
    
    #ifdef HAVE_SYS_PRCTL_H
        #include <sys/prctl.h>
    #endif

    #ifdef HAVE_CORESERVICES_CORESERVICES_H
        #include <CoreServices/CoreServices.h>
        #define LacewingUseMPSemaphore
    #else
        #include <semaphore.h>
    #endif

    #ifdef HAVE_OPENSSL_MD5_H
        #include <openssl/ssl.h>
        #include <openssl/md5.h>
        #include <openssl/sha.h>
        #include <openssl/err.h>
    #else
        #error "OpenSSL not found. Install OpenSSL and run ./configure again."
    #endif

    #define LacewingGetSocketError()  errno
    #define LacewingGetLastError()    errno

    inline void LacewingCloseSocket (int Socket)
    {
        if (Socket == -1)
            return;

        close (Socket);
    }

    #define _atoi64 atoll

    typedef int lw_socket;

    #define LacewingYield()  sched_yield()

    #define lw_vsnprintf                vsnprintf
    #define lw_snprintf                 snprintf

    #if HAVE_DECL_MSG_NOSIGNAL
        #define LacewingNoSignal MSG_NOSIGNAL
    #else
        #define LacewingNoSignal 0
    #endif

    namespace Lacewing
    {
        inline void DisableSigPipe (lw_socket Socket)
        {
            #if HAVE_DECL_SO_NOSIGPIPE
           
                int Yes = 1;
                setsockopt (Socket, SOL_SOCKET, SO_NOSIGPIPE, (char *) &Yes, sizeof (Yes));
            
            #endif
        }   
    }

#endif

#if defined(HAVE_MALLOC_H) || defined(LacewingWindows)
    #include <malloc.h>
#endif

inline int LacewingFormat(char *& Output, const char * Format, va_list args)
{
    #ifndef LacewingWindows

        /* TODO : Alternative for where vasprintf is not supported? */

        int Count = vasprintf(&Output, Format, args);

        if(Count == -1)
            Output = 0;

        return Count;

    #else

        int Count = _vscprintf(Format, args);

        Output = (char *) malloc(Count + 1);

        if(!Output)
            return 0;

        if(vsprintf (Output, Format, args) < 0)
        {
            free(Output);
            Output = 0;

            return 0;
        }

        return Count;

    #endif
}

#if defined(LacewingDebug) || defined(LacewingForceDebugOutput)

    extern Lacewing::Sync Sync_DebugOutput;

    inline void DebugOut (const char * format, ...)
    {
        va_list args;
        va_start (args, format);
        
        char * data;
        int size = LacewingFormat (data, format, args);
        
        if(size > 0)
        {
            Lacewing::Sync::Lock Lock (Sync_DebugOutput);

            #ifdef LacewingAndroid
                __android_log_write (ANDROID_LOG_INFO, "Lacewing", data);
            #else
                #ifdef COXSDK
                    OutputDebugString (data);
                    OutputDebugString ("\n");
                #else
                    printf ("[Lacewing] %s\n", data);
                #endif
            #endif

            free (data);
        }

        va_end (args);
    }

#else
    #define DebugOut(X, ...)
#endif

inline void LacewingSyncIncrement(volatile long * Target)
{
    #ifdef LacewingWindows
        InterlockedIncrement (Target);
    #else
        #ifdef __GNUC__
            __sync_add_and_fetch (Target, 1);
        #else
            #error "Don't know how to implement LacewingSyncIncrement on this platform"
        #endif
    #endif
}

inline void LacewingSyncDecrement(volatile long * Target)
{
    #ifdef LacewingWindows
        InterlockedDecrement (Target);
    #else
        #ifdef __GNUC__
            __sync_sub_and_fetch (Target, 1);
        #else
            #error "Don't know how to implement LacewingSyncDecrement on this platform"
        #endif
    #endif
}

inline long LacewingSyncCompareExchange(volatile long * Target, long NewValue, long OldValue)
{
    #ifdef LacewingWindows
        return InterlockedCompareExchange (Target, NewValue, OldValue);
    #else
        #ifdef __GNUC__
            return __sync_val_compare_and_swap (Target, OldValue, NewValue);
        #else
            #error "Don't know how to implement LacewingSyncCompareExchange on this platform"
        #endif
    #endif
}

inline void LacewingSyncExchange(volatile long * Target, long NewValue)
{
    #ifdef LacewingWindows
        InterlockedExchange (Target, NewValue);
    #else
        #ifdef __GNUC__
            
            for (;;)
            {
                long Current = *Target;

                if (__sync_val_compare_and_swap (Target, Current, NewValue) == Current)
                    break;
            }

        #else
            #error "Don't know how to implement LacewingSyncExchange on this platform"
        #endif
    #endif
}

namespace Lacewing
{
    inline void DisableNagling (lw_socket Socket)
    {
        int Yes = 1;
        setsockopt (Socket, SOL_SOCKET, TCP_NODELAY, (char *) &Yes, sizeof (Yes));
    }

    inline void DisableIPV6Only (lw_socket Socket)
    {
        int No = 0;
        setsockopt (Socket, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &No, sizeof (No));
    }

    inline int GetSocketPort (lw_socket Socket)
    {
        if (Socket == -1)
            return 0;

        sockaddr_storage Address;
        socklen_t AddressLength = sizeof (Address);

        if (getsockname (Socket, (sockaddr *) &Address, &AddressLength) == -1)
            return 0;

        return ntohs (Address.ss_family == AF_INET6 ?
            ((sockaddr_in6 *) &Address)->sin6_port : ((sockaddr_in *) &Address)->sin_port);
    }

    int CreateServerSocket (Lacewing::Filter &, int Type, int Protocol, Lacewing::Error &Error);

    inline int Read24Bit (const char * b)
    {
        return 65536 * b [0] + 256 * b [1] + b [2];
    }

    inline bool URLDecode (char * in, unsigned int in_length, char * out, unsigned int out_length)
    {
        char * out_end = out + out_length;
        char * in_end = in + in_length;

        while(in < in_end)
        {
            if(*in == '%')
            {
                if ((++ in) + 2 >= in_end)
                    return false;

                char n [3] = { in [0], in [1], 0 };
                *out = (char) strtol (n, 0, 16);

                ++ in;
            }
            else
                *out = *in == '+' ? ' ' : *in;

            if (out ++ >= out_end)
                return false;

            ++ in;
        }

        *out = 0;
        return true;
    }

    inline bool BeginsWith(const char * String, const char * Substring)
    {
        while(*Substring)
        {
            if(!*String || tolower(*String) != tolower(*Substring))
                return false;

            ++ Substring;
            ++ String;
        }

        return true;
    }

    inline char * Trim (const char * Input)
    {
        while (isspace (*Input))
            ++ Input;

        if (!*Input)
            return strdup ("");

        const char * End = Input + strlen (Input) - 1;
        int offset = 0;

        while (isspace (*(End --)))
            ++ offset;

        int Length = strlen (Input) - offset;

        char * Result = (char *) malloc (Length + 1);
        
        memcpy (Result, Input, Length); 
        Result [Length] = 0;

        return Result;
    }

    inline void CopyString (char * Dest, const char * Source, int Size)
    {
        int Length = strlen (Source);

        if (Length > -- Size)
            Length = Size;

        memcpy (Dest, Source, Length);
        Dest [Length] = 0;
    }
}

#include "Utility.h"
#include "TimeHelper.h"
#include "ReceiveBuffer.h"
#include "MessageBuilder.h"
#include "MessageReader.h"
#include "Backlog.h"
#include "Address.h"

#ifndef LacewingWindows
    #include "unix/EventPump.h"
#endif

#define AutoHandlerFunctions(Public, HandlerName)                        \
    void Public::on##HandlerName (Public::Handler##HandlerName Handler)   \
    {   internal->Handlers.HandlerName = Handler;                        \
    }                                                                    \

#define AutoHandlerFlat(real_class, flat, handler_upper, handler_lower) \
    void flat##_on##handler_lower (flat * _flat, flat##_handler_##handler_lower _handler) \
    {   ((real_class *) _flat)->on##handler_upper ((real_class::Handler##handler_upper) _handler); \
    }

#endif

