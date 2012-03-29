
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

Webserver::Request::Internal::Internal
    (Webserver::Internal &_Server, WebserverClient &_Client)
        : Server (_Server), Client (_Client)
{
    Public.Tag         = 0;
    Public.internal = this;

    FirstFile = 0;
}

Webserver::Request::Internal::~ Internal ()
{
}

void Webserver::Request::Internal::Clean ()
{
    /* HTTP clients reuse the same request object, so this should clear
       everything ready for a new request. */
    
    Responded = true;
   
    InHeaders   .Clear();
    InCookies   .Clear();
    GetItems    .Clear();
    PostItems   .Clear();

    *Method   = 0;
    *URL      = 0;
    *Hostname = 0;
    
    /* The output cookies are cleared here rather than in BeforeHandler() because
       the received cookies will be written to OutCookies as well as InCookies.  When
       it comes to sending the response, OutCookies will be compared to InCookies to
       see which cookies have changed and should be sent as Set-Cookie headers. */

    OutCookies.Clear();
}

void Webserver::Request::Internal::BeforeHandler ()
{
    /* Any preparation to be done immediately before calling the handler should be in this function */

    strcpy(Status, "200 OK");

    Response.Reset();     
    OutHeaders.Clear();
    
    TotalFileSize = TotalNonFileSize = 0;

    OutHeaders.Set ("Server", Lacewing::Version ());
    OutHeaders.Set ("Content-Type", "text/html; charset=UTF-8");

    if(Client.Secure)
    {
        /* When the request is secure, add the "Cache-Control: public" header by default.
           DisableCache() called in the handler will remove this, and should be used for any
           pages containing sensitive data that shouldn't be cached.  */

        OutHeaders.Set("Cache-Control", "public");
    }   

    LacewingAssert (Responded);

    Responded = false;
}

void Webserver::Request::Internal::AfterHandler ()
{
    /* Anything to be done immediately after the handler has returned should be in this function */
    
    if((!Responded) && Server.AutoFinish)
        Respond ();
}

void Webserver::Request::Internal::RunStandardHandler ()
{
    /* If the protocol doesn't want to call the handler itself (ie. it's a standard GET/POST/HEAD
       request), it will call this function to invoke the appropriate handler automatically. */

    BeforeHandler ();

    do
    {
        /* The BodyProcessor might want to call its own handler (eg for multipart file upload) */
        
        if(!strcmp(Method, "GET"))
        {
            if (Server.Handlers.Get)
                Server.Handlers.Get (Server.Webserver, Public);

            break;
        }

        if(!strcmp(Method, "POST"))
        {
            if (Server.Handlers.Post)
                Server.Handlers.Post (Server.Webserver, Public);

            break;
        }

        if(!strcmp(Method, "HEAD"))
        {
            if (Server.Handlers.Head)
                Server.Handlers.Head (Server.Webserver, Public);

            break;
        }

        Public.Status (501, "Not Implemented");
        Public.Finish ();

        return;

    } while(0);

    AfterHandler ();
}

void Webserver::Request::Internal::In_Method
    (const char * method)
{
    CopyString (Method, method, sizeof (Method));
}

void Webserver::Request::Internal::In_Header
        (const char * Name, char * Value)
{
    if (!strcasecmp(Name, "Cookie"))
    {
        for(;;)
        {
            char * CookieName  = Value;
            char * CookieValue = Value;

            if(!(CookieValue = strchr(CookieValue, '=')))
                break; /* invalid cookie */
            
            *(CookieValue ++) = 0;

            char * Next = strchr(CookieValue, ';');

            if(Next)
                *(Next ++) = 0;

            while(*CookieName == ' ')
                ++ CookieName;

            while(*CookieName && CookieName[strlen(CookieName - 1)] == ' ')
                CookieName[strlen(CookieName - 1)] = 0;

            /* The copy in InCookies doesn't get modified, so the response generator
               can determine which cookies have been changed. */

            InCookies.Set (CookieName, CookieValue);
            OutCookies.Set (CookieName, CookieValue);
            
            if(!(Value = Next))
                break;
        }

        /* TODO : Keep the raw header too? */

        return;
    }

    if(!strcasecmp (Name, "Host"))
    {
        /* The hostname gets stored separately with the port removed for
           the Request.Hostname() function (the raw header is still saved) */

        CopyString(this->Hostname, Value, sizeof (this->Hostname));

        for(char * i = this->Hostname; *i; ++ i)
        {
            if(*i == ':')
            {
                *i = 0;
                break;
            }
        }
    }   
    
    InHeaders.Set (Name, Value);
}

static inline bool GetField (char * URL, http_parser_url &parsed, int field, char * &buf, int &length)
{
    if (! (parsed.field_set & (1 << field)))
        return false;

    buf = URL + parsed.field_data [field].off;
    length = parsed.field_data [field].len;

    return true;
}

bool Webserver::Request::Internal::In_URL (char * URL)
{
    /* Check for any directory traversal in the URL, and replace
     * any backward slashes with forward slashes.
     */

    for (char * i = URL; *i; ++ i)
    {
        if(i [0] == '.' && i [1] == '.')
            return false;

        if(*i == '\\')
            *i = '/';
    }

    /* Now hand it over to the URL parser */

    http_parser_url parsed;

    if (http_parser_parse_url (URL, strlen (URL), 0, &parsed))
        return false;


    /* Path */

    *this->URL = 0;

    {   char * path;
        int path_length;

        if (GetField (URL, parsed, UF_PATH, path, path_length))
        {
            if (*path == '/')
            {
                ++ path;
                -- path_length;
            }

            char * end = path + path_length, b = *end;

            if (!URLDecode (path, path_length, this->URL, sizeof (this->URL)))
            {
                *end = b;
                return false;
            }
            
            *end = b;
        }
    }


    /* Host */

    {   char * host;
        int host_length;

        if (GetField (URL, parsed, UF_HOST, host, host_length))
        {
            if (parsed.field_set & (1 << UF_PORT))
            {
                /* TODO : Is this robust? */

                host_length += 1 + parsed.field_data [UF_PORT].off;
            }

            char * end = host + host_length, b = *end;

            *end = 0;
            In_Header ("Host", host);
            *end = b;
        }
    }


    /* GET data */

    {   char * get_data;
        int get_data_length;

        if (GetField (URL, parsed, UF_QUERY, get_data, get_data_length))
        {
            char * end = get_data + get_data_length, b = *end;

            *end = 0;

            for (;;)
            {
                char * name = get_data, * value = strchr (get_data, '=');

                if (!value ++)
                    break;

                char * next = strchr (value, '&');

                int name_length = (value - name) - 1,
                    value_length = next ? next - value : strlen (value);

                char * name_decoded = (char *) malloc (name_length + 1),
                     * value_decoded = (char *) malloc (value_length + 1);

                if(!URLDecode (name, name_length, name_decoded, name_length + 1)
                        || !URLDecode (value, value_length, value_decoded, value_length + 1))
                {
                    free (name_decoded);
                    free (value_decoded);
                }
                else
                    GetItems.Set (name_decoded, value_decoded, false);

                if (!next)
                    break;

                get_data = ++ next;
            }

            *end = b;
        }
    }

    return true;
}

void Webserver::Request::Internal::AddFileSend (const char * Filename, lw_i64 FileOffset, lw_i64 FileSize)
{
    Webserver::Request::Internal::File * File = FirstFile;

    if(!File)
    {
        File = FirstFile = new Webserver::Request::Internal::File;
    }
    else
    {
        while(File->Next)
            File = File->Next;

        File->Next = new Webserver::Request::Internal::File;
        File = File->Next;
    }

    File->Next       = 0;
    File->Offset     = Response.Size;
    File->FileOffset = FileOffset;
    File->FileSize   = FileSize;

    CopyString (File->Filename, Filename, sizeof (File->Filename));
}

void Webserver::Request::Internal::File::Send (Server::Client &Socket, int ToSend, bool &Flushed)
{
    if (ToSend != -1)
    {
        if(*Filename)
        {
            /* TODO: Hmm, it's a shame it has to reopen the file every time the SPDY
               window is filled */

            Socket.SendFile (Filename, FileOffset, ToSend);
            Flushed = true;
            
            FileOffset += ToSend;
            FileSize   -= ToSend;
            
            return;
        }

        Socket.Send ((char *) FileOffset, ToSend);
            
        FileOffset += ToSend;
        FileSize   -= ToSend;

        return;
    }

    if(*Filename)
    {
        Socket.SendFile (Filename, FileOffset, FileSize);
        Flushed = true;

        return;
    }

    Socket.Send ((char *) FileOffset, (int) FileSize);
}

void Webserver::Request::Internal::Respond ()
{
    LacewingAssert (!Responded);

    Client.Respond (*this);
    Responded = true;
}

Address &Webserver::Request::GetAddress ()
{
    return ((Webserver::Request::Internal *) internal)->Client.Socket.GetAddress ();
}

void Webserver::Request::Disconnect ()
{
    ((Webserver::Request::Internal *) internal)->Client.Socket.Disconnect ();
}

void Webserver::Request::Send (const char * Data, int Size)
{
    if(Size == -1)
        Size = strlen(Data);

    if (Size <= 0)
        return;

    internal->TotalNonFileSize += Size;
    internal->Response.Add (Data, Size);
}

void Webserver::Request::SendConstant (const char * Data, int Size)
{
    if(Size == -1)
        Size = strlen(Data);

    if (Size <= 0)
        return;

    internal->TotalNonFileSize += Size;
    internal->AddFileSend ("", (lw_i64) Data, Size);
}

void Webserver::Request::SendFile (const char * Filename, lw_i64 Offset, lw_i64 Size)
{
    if (!*Filename)
        return;

    if (Size == -1)
        Size = FileSize (Filename);

    if (Size <= 0)
        return;

    internal->TotalFileSize += Size;
    internal->AddFileSend (Filename, Offset, Size);
}

void Webserver::Request::Reset ()
{
    internal->Response.Reset ();
    
    {   Webserver::Request::Internal::File * File = internal->FirstFile;

        while(File)
        {
            delete File;
            File = File->Next;
        }

        internal->FirstFile = 0;
    }

    internal->TotalFileSize = 0;
    internal->TotalNonFileSize = 0;
}

void Webserver::Request::GuessMimeType (const char * Filename)
{
    SetMimeType(Lacewing::GuessMimeType(Filename));
}

void Webserver::Request::SetMimeType (const char * MimeType, const char * Charset)
{
     if (!*Charset)
     {
         Header ("Content-Type", MimeType);
         return;
     }

     char Type [256];

     lw_snprintf (Type, sizeof (Type), "%s; charset=%s", MimeType, Charset);
     Header ("Content-Type", Type);
}

void Webserver::Request::SetRedirect (const char * URL)
{
    Status (303, "See Other");
    Header ("Location", URL);
}

void Webserver::Request::DisableCache ()
{
    Header("Cache-Control", "no-cache");
}

void Webserver::Request::Header (const char * Name, const char * Value)
{
    ((Webserver::Request::Internal *) internal)->OutHeaders.Set (Name, Value);
}

void Webserver::Request::Cookie (const char * Name, const char * Value)
{
    Cookie (Name, Value, Secure () ? "Secure; HttpOnly" : "HttpOnly");
}

void Webserver::Request::Cookie (const char * Name, const char * Value, const char * Attributes)
{
    if (!*Attributes)
    {
        ((Webserver::Request::Internal *) internal)->OutCookies.Set (Name, Value);
        return;
    }

    char * Buffer = (char *) malloc (strlen (Value) + strlen (Attributes) + 4);

    strcpy (Buffer, Value);
    strcat (Buffer, "; ");
    strcat (Buffer, Attributes);

    ((Webserver::Request::Internal *) internal)->OutCookies.Set (strdup (Name), Buffer, false);
}

void Webserver::Request::Status (int Code, const char * Message)
{
    sprintf (((Webserver::Request::Internal *) internal)->Status, "%d %s", Code, Message);
}

void Webserver::Request::SetUnmodified ()
{
    Status (304, "Not Modified");
}

void Webserver::Request::LastModified (lw_i64 Time)
{
    tm TM;

    time_t TimeT = (time_t) Time;
    gmtime_r(&TimeT, &TM);

    char LastModified [128];
    sprintf (LastModified, "%s, %02d %s %d %02d:%02d:%02d GMT", Weekdays [TM.tm_wday], TM.tm_mday,
                            Months [TM.tm_mon], TM.tm_year + 1900, TM.tm_hour, TM.tm_min, TM.tm_sec);

    Header("Last-Modified", LastModified);
}

void Webserver::Request::Finish ()
{
    ((Webserver::Request::Internal *) internal)->Respond ();
}

const char * Webserver::Request::Header (const char * Name)
{
    return ((Webserver::Request::Internal *) internal)->InHeaders.Get (Name);
}

struct Webserver::Request::Header * Webserver::Request::FirstHeader ()
{
    return (struct Webserver::Request::Header *)
                ((Webserver::Request::Internal *) internal)->InHeaders.First;
}

const char * Webserver::Request::Header::Name ()
{
    return ((Map::Item *) this)->Key;
}

const char * Webserver::Request::Header::Value ()
{
    return ((Map::Item *) this)->Value;
}

struct Webserver::Request::Header * Webserver::Request::Header::Next ()
{
    return (struct Webserver::Request::Header *) ((Map::Item *) this)->Next;
}

const char * Webserver::Request::Cookie (const char * Name)
{
    return ((Webserver::Request::Internal *) internal)->InCookies.Get (Name);
}

struct Webserver::Request::Cookie * Webserver::Request::FirstCookie ()
{
    return (struct Webserver::Request::Cookie *)
                ((Webserver::Request::Internal *) internal)->InCookies.First;
}

struct Webserver::Request::Cookie * Webserver::Request::Cookie::Next ()
{
    return (struct Webserver::Request::Cookie *) ((Map::Item *) this)->Next;
}

const char * Webserver::Request::Cookie::Name ()
{
    return ((Map::Item *) this)->Key;
}

const char * Webserver::Request::Cookie::Value ()
{
    return ((Map::Item *) this)->Value;
}

const char * Webserver::Request::GET (const char * Name)
{
    return ((Webserver::Request::Internal *) internal)->GetItems.Get (Name);
}

const char * Webserver::Request::POST (const char * Name)
{
    return ((Webserver::Request::Internal *) internal)->PostItems.Get (Name);
}

Webserver::Request::Parameter * Webserver::Request::GET ()
{
    return (Webserver::Request::Parameter *)
                ((Webserver::Request::Internal *) internal)->GetItems.First;
}

Webserver::Request::Parameter * Webserver::Request::POST ()
{
    return (Webserver::Request::Parameter *)
                ((Webserver::Request::Internal *) internal)->PostItems.First;
}

Webserver::Request::Parameter *
        Webserver::Request::Parameter::Next ()
{
    return (Webserver::Request::Parameter *) ((Map::Item *) this)->Next;
}

const char * Webserver::Request::Parameter::Name ()
{
    return ((Map::Item *) this)->Key;
}

const char * Webserver::Request::Parameter::Value ()
{
    return ((Map::Item *) this)->Value;
}

lw_i64 Webserver::Request::LastModified ()
{
    const char * LastModified = Header("If-Modified-Since");

    if(*LastModified)
        return ParseTimeString(LastModified);

    return 0;
}

bool Webserver::Request::Secure ()
{
    return ((Webserver::Request::Internal *) internal)->Client.Secure;
}

const char * Webserver::Request::Hostname ()
{
    return ((Webserver::Request::Internal *) internal)->Hostname;
}

const char * Webserver::Request::URL ()
{
    return ((Webserver::Request::Internal *) internal)->URL;
}

int Webserver::Request::IdleTimeout ()
{
    return ((Webserver::Request::Internal *) internal)->Client.Timeout;
}

void Webserver::Request::IdleTimeout (int Seconds)
{
    ((Webserver::Request::Internal *) internal)->Client.Timeout = Seconds;
}
