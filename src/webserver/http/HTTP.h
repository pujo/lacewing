
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

class HTTPClient : public WebserverClient
{
    Webserver::Request::Internal Request; /* HTTP is one request at a time, so this is just reused */
    
    time_t LastActivityTime;

    http_parser Parser;
    bool ParsingHeaders, SignalEOF;
    
    MessageBuilder Buffer;
    
    char * CurHeaderName;
    size_t CurHeaderNameLength;

public:

    HTTPClient (Webserver::Internal &, Lacewing::Server::Client &, bool Secure);
    ~ HTTPClient ();

    void Process (char * Buffer, int Size);

    /* Called by the HTTP parser */

    int onMessageBegin ();
    int onHeadersComplete ();
    int onMessageComplete ();

    int onURL (char *, size_t);
    int onBody (char *, size_t);
    int onHeaderField (char *, size_t);
    int onHeaderValue (char *, size_t);

    void Respond (Webserver::Request::Internal &);
    void Dead ();

    void Tick ();

    bool IsSPDY ();

    struct HTTPUpload : public Webserver::Upload::Internal
    {
        const char * Header (const char * Name);
    };

    struct MultipartProcessor
    {
        enum { Error, Continue, Done } State;

        HTTPClient &Client;

        char Boundary         [256];
        char FinalBoundary    [256];
        char CRLFThenBoundary [256];

        bool   InHeaders, InFile;
        char * Header;
        void   ProcessHeader();

        MultipartProcessor * Child, * Parent;

        MultipartProcessor (HTTPClient &, const char * ContentType);
        ~ MultipartProcessor ();

        int Process (char *, size_t);
        void CallRequestHandler ();

        void ProcessDispositionPair(char * Pair);
        void ToFile(const char *, size_t);

        Map Disposition, Headers;

        Array <Lacewing::Webserver::Upload *> Uploads;
        HTTPUpload * CurrentUpload;

        MessageBuilder Buffer;

    } * Multipart;
};

