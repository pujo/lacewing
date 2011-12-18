liblacewing is a library for writing cross-platform, networked applications in C,
C++ and Javascript.

http://lacewing-project.org/

http://github.com/udp/lacewing/


Installing
==========

On Windows, use the MSVC project file in the `msvc` directory to build a DLL/LIB.

On Linux/BSD/OS X:

    ./configure
    make
    make install

For other platforms (such as the Android NDK) see the documentation.

Ubuntu users can also add the following to their software sources to get the `liblacewing` and `liblacewing-dev` packages:

    deb http://ppa.launchpad.net/udp/ppa/ubuntu natty main 
    deb-src http://ppa.launchpad.net/udp/ppa/ubuntu natty main 


Documentation
=============

The API documentation for liblacewing can be found [here](http://lacewing-project.org/docs).

Issues with the documentation can be reported in the [gh-pages branch](https://github.com/udp/lacewing/tree/gh-pages)
of the Lacewing GitHub repository.


Changes in 0.2.6 (2011-Dec-18)
==============================

- `Webserver::Request::Cookie` now has a parameter allowing you to set attributes for each cookie.  By default,
  both the `Secure` and `HttpOnly` attributes are set.

- You can now iterate through the headers, cookies, GET/POST parameters and session data stored in a `Webserver::Request`

- Bug when parsing fragmented HTTP requests fixed.  This made `Webserver` incompatible with recent fixes for the SSL
  BEAST attack.

- The library now builds as `liblacewing.dylib` on OS X instead of `liblacewing.so`

- The source should now build using MinGW (although the included makefile will not work on Windows)


Changes in 0.2.5 (2011-Oct-11)
==============================

- Resolve various issues with the build system


Changes in 0.2.4 (2011-Oct-01)
==============================

- Some functions were missing `LacewingFunction` in Lacewing.h, and thus were not correctly
  exported from the shared object/DLL :-

    * RelayServer::Channel::Next
    * RelayServer::Client::Next
    * RelayServer::ChannelCount
    * RelayServer::SetChannelListing

- The name of a client was set *before* calling the `onNameSet` handler in RelayServer

- Client disconnect issues on BSD/OS X fixed in Server

- `MSG_NOSIGNAL` or `SO_NOSIGPIPE` are now used everywhere, to prevent the generation of
  unwanted SIGPIPE signals on Unix

- Possible EventPump deadlock fixed (Unix)

- New functions introduced:

    * Client::CheapBuffering
    * UDP::Hosting

- FlashPlayerPolicy class renamed to FlashPolicy

- Miscellaneous fixes in the C API 


Changes in 0.2.3 (2011-Sep-07)
==============================

- Fixed an issue with Lacewing::EventPump on Windows, which would cause functions queued with
  Post() not to execute in some cases.  This lead to to a significant issue with client disconnects
  not being detected properly in Lacewing::Server.
  
- Miscellaneous relay protocol fixes

    
Changes in 0.2.2 (2011-Aug-31)
==============================

- Lacewing::Timer now ends the timer thread properly in the destructor.  In previous
  versions it was necessary to call Stop() explicitly to avoid a crash.
  
- Two issues with the EventPump class fixed:

    * StartEventLoop() could return prematurely in some cases (Unix only)
    * Possible segmentation fault when a null WriteCallback was specified

- Added the RelayClient and RelayServer source files to the Makefile and Android.mk

- SHA1_Hex no longer cuts off the last four bytes

- Updated relay protocol specification and implementation classes (now revision 3)

- Fixed issue with the GuessMimeType function returning application/octet-stream even
  for known file extensions

- Refactored the internals of Lacewing::Webserver for future SPDY support.  The HTTP
  specific code is now separate from the webserver itself.


Changes in 0.2.1 (2011-Jul-15)
==============================

- Fixed crash in the List utility class introduced by 0.2.0


Changes in 0.2.0 (2011-Jul-14)
==============================

- New Javascript liblacewing API for MozJS (SpiderMonkey/TraceMonkey) and V8.
  Currently only supports the webserver and utility classes.  It is also
  now possible to build liblacewing as a Node.JS module - a wscript for node-waf
  is provided.
  
- Added support for building for Android (Android.mk is in the 'jni' folder)

- The build system has been changed from automake to autoconf with a simple
  Makefile.  This should solve the headaches with building on different
  platforms (Windows users should still use the MSVC project as normal.) 

- Removed the dependency on STL, resulting in a much faster build and smaller
  binaries (particularly on Android.)  This also makes it easier to link
  liblacewing into static Linux binaries.

- New global hash functions, implemented via OpenSSL or the Win32 crypt32 API:

    * Lacewing::MD5
    * Lacewing::MD5_Hex
    * Lacewing::SHA1
    * Lacewing::SHA1_Hex
    
- PostEventLoopExit() function added to the EventPump, to cause StartEventLoop()
  to return (once any pending events have been processed.)

- In the Webserver, Request objects are now freed immediately after the
  connection is closed, after calling the disconnect handler.  Previously, the
  Request object would remain in a disconnected state until Finish() was called.

- In Unix, the EventPump class is is now just a default implementation of the
  Pump class, which can be derived from to use custom methods of watching FDs.
  A custom pump using libev is provided in the 'ev' folder.

- Fixed some issues with Lacewing::Server on Unix:

    * SendFile didn't always complete in SSL mode
    * SendFile didn't send the file if open() returned 0
    * The Disconnect handler didn't always trigger

    
