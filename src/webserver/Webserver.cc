
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

#include "Common.h"

WebserverClient::WebserverClient (Webserver::Internal &_Server, Server::Client &_Socket, bool _Secure)
    : Server (_Server), Socket (_Socket), Secure (_Secure), Timeout (_Server.Timeout)
{
}

char * Webserver::Internal::BorrowSendBuffer ()
{
    if (!SendBuffers.First)
        for (int i = Webserver::Internal::SendBufferBacklog; i; -- i)
            SendBuffers.Push (new char [Webserver::Internal::SendBufferSize]);

    return SendBuffers.Pop ();
}

void Webserver::Internal::ReturnSendBuffer (char * SendBuffer)
{
    SendBuffers.Push (SendBuffer);
}

void Webserver::Internal::SocketConnect (Server &Server, Server::Client &Client)
{
}

void Webserver::Internal::SocketDisconnect (Server &Server, Server::Client &Client)
{
    if (!Client.Tag)
        return;
    
    ((WebserverClient *) Client.Tag)->Dead ();

    delete ((WebserverClient *) Client.Tag);
}

void Webserver::Internal::SocketReceive (Server &Server, Server::Client &Client, char * Buffer, int Size)
{
    Webserver::Internal &Webserver = *(Webserver::Internal *) Server.Tag;

    if (!Client.Tag)
        Client.Tag = (WebserverClient *) new HTTPClient (Webserver, Client, &Server == Webserver.SecureSocket);

    ((WebserverClient *) Client.Tag)->Process(Buffer, Size);
}

void Webserver::Internal::SocketError (Server &Server, Error &Error)
{
    Error.Add("Socket error");

    Webserver::Internal &Webserver = *(Webserver::Internal *) Server.Tag;

    if (Webserver.Handlers.Error)
        Webserver.Handlers.Error (Webserver.Webserver, Error);
}

void Webserver::Internal::TimerTickStatic (Lacewing::Timer &Timer)
{
    ((Webserver::Internal *) Timer.Tag)->TimerTick ();
}

void Webserver::Internal::TimerTick ()
{
    if (Socket)
    {
        for (Server::Client * Client =
                Socket->FirstClient (); Client; Client = Client->Next ())
        {
            if (!Client->Tag)
                continue;

            ((WebserverClient *) Client->Tag)->Tick ();
        }
    }

    if (SecureSocket)
    {
        for (Server::Client * Client =
                SecureSocket->FirstClient (); Client; Client = Client->Next ())
        {
            if (!Client->Tag)
                continue;

            ((WebserverClient *) Client->Tag)->Tick ();
        }
    }
}

Webserver::Webserver (Lacewing::Pump &Pump)
{
    LacewingInitialise();
    
    internal = new Webserver::Internal (*this, Pump);
    Tag = 0;
}

Webserver::~Webserver ()
{
    Unhost();
    UnhostSecure();

    delete internal;
}

void Webserver::Host (int Port)
{
    Filter Filter;
    Filter.LocalPort(Port);

    Host(Filter);
}

void Webserver::Host (Filter &Filter)
{
    internal->PrepareSocket ();

    if (!Filter.LocalPort())
        Filter.LocalPort(80);

    internal->Socket->Host(Filter, true);
}

void Webserver::HostSecure (int Port)
{
    Filter Filter;
    Filter.LocalPort(Port);

    HostSecure(Filter);
}

void Webserver::HostSecure (Filter &Filter)
{
    if(!CertificateLoaded())
        return;

    internal->PrepareSecureSocket();

    if (!Filter.LocalPort())
        Filter.LocalPort(443);

    internal->SecureSocket->Host(Filter, true);
}

void Webserver::Unhost ()
{
    if (internal->Socket)
        internal->Socket->Unhost ();
}

void Webserver::UnhostSecure ()
{
    if (internal->SecureSocket)
        internal->SecureSocket->Unhost ();
}

bool Webserver::Hosting ()
{
    if (!internal->Socket)
        return false;

    return internal->Socket->Hosting ();
}

bool Webserver::HostingSecure ()
{
    if (!internal->SecureSocket)
        return false;

    return internal->SecureSocket->Hosting ();
}

int Webserver::Port ()
{
    if (!internal->Socket)
        return 0;

    return internal->Socket->Port ();
}

int Webserver::SecurePort ()
{
    if (!internal->SecureSocket)
        return 0;

    return internal->SecureSocket->Port ();
}

lw_i64 Webserver::BytesSent ()
{
    return (internal->Socket ? internal->Socket->BytesSent () : 0)
        + (internal->SecureSocket ? internal->SecureSocket->BytesSent () : 0);
}

lw_i64 Webserver::BytesReceived ()
{
    return (internal->Socket ? internal->Socket->BytesReceived () : 0)
        + (internal->SecureSocket ? internal->SecureSocket->BytesReceived () : 0);
}

bool Webserver::LoadCertificateFile (const char * Filename, const char * CommonName)
{
    internal->PrepareSecureSocket ();

    return internal->SecureSocket->LoadCertificateFile (Filename, CommonName);
}

bool Webserver::LoadSystemCertificate (const char * StoreName, const char * CommonName, const char * Location)
{
    internal->PrepareSecureSocket ();

    return internal->SecureSocket->LoadSystemCertificate (StoreName, CommonName, Location);
}

bool Webserver::CertificateLoaded ()
{
    internal->PrepareSecureSocket ();

    return internal->SecureSocket->CertificateLoaded ();
}

void Webserver::EnableManualRequestFinish ()
{
    internal->AutoFinish = false;
}

void Webserver::IdleTimeout (int Seconds)
{
    internal->Timeout = Seconds;

    if (internal->Timer.Started ())
    {
        internal->StopTimer ();
        internal->StartTimer ();
    }
}

int Webserver::IdleTimeout ()
{
    return internal->Timeout;
}

const char * Webserver::Upload::Filename ()
{
    return internal->Filename;
}

const char * Webserver::Upload::FormElementName ()
{
    return internal->FormElement;
}

const char * Webserver::Upload::Header (const char * Name)
{
    return internal->Header (Name);
}

struct Webserver::Upload::Header * Webserver::Upload::FirstHeader ()
{
    return (struct Webserver::Upload::Header *)
                internal->Headers.First;
}

const char * Webserver::Upload::Header::Name ()
{
    return ((Map::Item *) this)->Key;
}

const char * Webserver::Upload::Header::Value ()
{
    return ((Map::Item *) this)->Value;
}

struct Webserver::Upload::Header * Webserver::Upload::Header::Next ()
{
    return (struct Webserver::Upload::Header *) ((Map::Item *) this)->Next;
}

void Webserver::Upload::SetAutoSave ()
{
    if (*internal->AutoSaveFilename)
        return;

    char Filename [lw_max_path];
    NewTempFile (Filename);

    internal->AutoSaveFilename = internal->Copier.Set ("AutoSaveFilename", Filename)->Value;
    internal->AutoSaveFile     = fopen (Filename, "wb");
}

const char * Webserver::Upload::GetAutoSaveFilename ()
{
    return internal->AutoSaveFilename;
}

AutoHandlerFunctions (Webserver, Get)
AutoHandlerFunctions (Webserver, Post)
AutoHandlerFunctions (Webserver, Head)
AutoHandlerFunctions (Webserver, Error)
AutoHandlerFunctions (Webserver, UploadStart)
AutoHandlerFunctions (Webserver, UploadChunk)
AutoHandlerFunctions (Webserver, UploadDone)
AutoHandlerFunctions (Webserver, UploadPost)
AutoHandlerFunctions (Webserver, Disconnect)

