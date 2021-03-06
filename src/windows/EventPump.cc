
/* vim: set et ts=4 sw=4 ft=cpp:
 *
 * Copyright (C) 2011 James McLaughlin.  All rights reserved.
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

#include "../Common.h"

#define SigExitEventLoop     ((OVERLAPPED *) 1)
#define SigRemove            ((OVERLAPPED *) 2)
#define SigEndWatcherThread  ((OVERLAPPED *) 3)

struct EventPump::Internal
{  
    Lacewing::EventPump &Pump;
    Lacewing::Thread WatcherThread;

    HANDLE CompletionPort;

    Internal (Lacewing::EventPump &_Pump)
        : Pump (_Pump), WatcherThread ("Watcher", (void *) Watcher)
    {
        CompletionPort     = CreateIoCompletionPort (INVALID_HANDLE_VALUE, 0, 4, 0);
        HandlerTickNeeded  = 0;
    }

    struct Event
    {
        void * Tag;

        Pump::Callback Callback;
    };

    Backlog <Event> EventBacklog;

    void (LacewingHandler * HandlerTickNeeded) (Lacewing::EventPump &EventPump);

    static bool Process (EventPump::Internal * internal, OVERLAPPED * Overlapped,
                            unsigned int BytesTransferred, EventPump::Internal::Event * Event, int Error)
    {
        if (BytesTransferred == 0xFFFFFFFF)
        {
            /* See Post () */

            ((void * (*) (void *)) Event) (Overlapped);

            return true;
        }

        if (Overlapped == SigRemove)
        {
            internal->EventBacklog.Return (*Event);
            internal->Pump.RemoveUser ();

            return true;
        }

        if (Overlapped == SigExitEventLoop)
            return false;

        if (Event->Callback)
            Event->Callback (Event->Tag, Overlapped, BytesTransferred, Error);

        return true;
    }
    
    OVERLAPPED * WatcherOverlapped;
    unsigned int WatcherBytesTransferred;
    EventPump::Internal::Event * WatcherEvent;
    Lacewing::Event WatcherResumeEvent;
    int WatcherError;

    static void Watcher (EventPump::Internal * internal)
    {
        for (;;)
        {
            internal->WatcherError = 0;

            if (!GetQueuedCompletionStatus (internal->CompletionPort, (DWORD *) &internal->WatcherBytesTransferred,
                     (PULONG_PTR) &internal->WatcherEvent, &internal->WatcherOverlapped, INFINITE))
            {
                if ((internal->WatcherError = GetLastError ()) == WAIT_TIMEOUT)
                    break;

                if (!internal->WatcherOverlapped)
                    break;

                internal->WatcherBytesTransferred = 0;
            }

            if (internal->WatcherOverlapped == SigEndWatcherThread)
                break;

            internal->HandlerTickNeeded (internal->Pump);

            internal->WatcherResumeEvent.Wait ();
            internal->WatcherResumeEvent.Unsignal ();
        }
    }
};

EventPump::EventPump (int)
{
    internal = new EventPump::Internal (*this);
    Tag = 0;
}

EventPump::~EventPump ()
{
    if (internal->WatcherThread.Started ())
    {
        PostQueuedCompletionStatus (internal->CompletionPort, 0, 0, SigEndWatcherThread);

        internal->WatcherResumeEvent.Signal ();
        internal->WatcherThread.Join ();
    }

    delete internal;
}

Error * EventPump::Tick ()
{
    if (internal->HandlerTickNeeded)
    {
        /* Process whatever the watcher thread dequeued before telling the caller to tick */

        Internal::Process (internal, internal->WatcherOverlapped, internal->WatcherBytesTransferred,
                            internal->WatcherEvent, internal->WatcherError); 
    }

    OVERLAPPED * Overlapped;
    unsigned int BytesTransferred;
    
    EventPump::Internal::Event * Event;

    for(;;)
    {
        int Error = 0;

        if (!GetQueuedCompletionStatus (internal->CompletionPort, (DWORD *) &BytesTransferred,
                 (PULONG_PTR) &Event, &Overlapped, 0))
        {
            Error = GetLastError();

            if(Error == WAIT_TIMEOUT)
                break;

            if(!Overlapped)
                break;
        }

        Internal::Process (internal, Overlapped, BytesTransferred, Event, Error);
    }
 
    if (internal->HandlerTickNeeded)
        internal->WatcherResumeEvent.Signal ();

    return 0;
}

Error * EventPump::StartEventLoop ()
{
    OVERLAPPED * Overlapped;
    unsigned int BytesTransferred;
    
    EventPump::Internal::Event * Event;
    bool Exit = false;

    for(;;)
    {
        /* TODO : Use GetQueuedCompletionStatusEx where available */

        int Error = 0;

        if (!GetQueuedCompletionStatus (internal->CompletionPort, (DWORD *) &BytesTransferred,
                 (PULONG_PTR) &Event, &Overlapped, INFINITE))
        {
            Error = GetLastError();

            if(!Overlapped)
                continue;
        }

        if (!Internal::Process (internal, Overlapped, BytesTransferred, Event, Error))
            break;
    }
        
    return 0;
}

void EventPump::PostEventLoopExit ()
{
    PostQueuedCompletionStatus (internal->CompletionPort, 0, 0, SigExitEventLoop);
}

Error * EventPump::StartSleepyTicking (void (LacewingHandler * onTickNeeded) (EventPump &EventPump))
{
    internal->HandlerTickNeeded = onTickNeeded;    
    internal->WatcherThread.Start (internal);

    return 0;
}

void * EventPump::Add (HANDLE Handle, void * Tag, Pump::Callback Callback)
{
    LacewingAssert (Callback != 0);

    Internal::Event &E = internal->EventBacklog.Borrow ();

    E.Callback = Callback;
    E.Tag = Tag;

    if (CreateIoCompletionPort (Handle,
            internal->CompletionPort, (ULONG_PTR) &E, 0) != 0)
    {
        /* success */
    }
    
    AddUser ();

    return &E;
}

void EventPump::Remove (void * Key)
{
    ((Internal::Event *) Key)->Callback = 0;

    PostQueuedCompletionStatus
        (internal->CompletionPort, 0, (ULONG_PTR) Key, SigRemove);
}

bool EventPump::IsEventPump ()
{
    return true;
}

void EventPump::Post (void * Function, void * Parameter)
{
    PostQueuedCompletionStatus
        (internal->CompletionPort, 0xFFFFFFFF,
            (ULONG_PTR) Function, (OVERLAPPED *) Parameter);
}

