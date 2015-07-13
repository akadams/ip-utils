$Id: README.txt,v 1.2 2013/05/07 13:29:08 akadams Exp $

IP-Utils Library: Andrew K. Adams <akadams@psc.edu>


OVERVIEW

IP-Utils is a collection of C++ Classes that enable data communication via a simple API for various IP-based networking protocols.  Designed as a layered architecture (mimicking the ISO layers), it uses file descriptor reference counting to provide safe copying of networking objects.  An application requests either a half-tuple (e.g., TCPConn tcp_connection;) or an entire flow (e.g., TCPSession tcp_session(FRAMING_TYPE);) and communicates data over those objects using a specific message framing (e.g., HTTPFraming).  Different message framing is achieved within flows by encapsulating the different framing headers within a single Class (i.e., MsgHdr), which the flow Classes (e.g., TCPSession) interface with.  The IP-Util Classes can be used individually, or the collection can be built into a library archive.

Additional utility Classes for error handling, logging, URL handling and file management are also included and used by the networking Classes.  Since C++ exception handling is *not* looked on favorably, error handling is done through the ErrorHandling Class, which provides a global structure that Classes can use to initiate and append to *events* for processing by the application.  Likewise, logging is done through the Logger Class, which similarly uses a mechanism based on a global object.  The Logger object allows Classes to report events by both priority (syslog(3)) and mechanism (e.g., stderr, syslog, file, script), however, logging will *not* be enabled unless the code that links with IP-Utils is complied using the flag "-DUSE_LOGGER".  A minimal Class to parse and generate URLs is used by the HTTPFraming class, referred to as URL.  And finally, the File Class handles all disk I/O used by the networking Classes, both low-level I/O and streaming FILE* I/O.  All four non-networking utility Classes can be used outside of IP-Utils, if so desired.  Features of IP-Utils Classes, include:

 - DOxygen comments included in header files.  - Handles IPv4 or IPv6 communications.  - Provides TCP or UDP objects.  - Currently supports *struct-based* and HTTP framing (including MIME-type handling).  - Code written to Google C++ Style Guide <>


The layered design of IP-Utils allows easy expansion.  For example, with little work a UDP flow Class could be generated (technically, UDP is connection-less, however, flows can exist nonetheless).  Moreover, multiple message framing could be supported by IP-Utils simply by adding the appropriate Class (e.g., SNMP) and adding the hooks within the MsgHdr Class.


BUGS

- The HTTPFraming Class supports HTTP 1.1 specification, however, it does not yet handle data chunking and some redirection/proxy commands.


EXAMPLES

A simple iterative HTTP server that processes a received TCP flow (TCPSession objects) using IP-Utils:

#include <err.h>
#include <sysexits.h>

#include "TCPSession.h"

int main(int argc, char* argv[]) 
{
  logger.set_mechanims(Logger::LOG_TO_STDERR);  // set Logger to use STDERR

  // Setup server.
  TCPConn server;  // our server's half-tuple
  server.InitServer(AF_INET);  // listen on all IPv4 interfaces
  server.set_blocking();  // make server non-blocking
  server.set_close_on_exec();  // if we fork() and exec(), close the socket
  server.Socket(PF_INET, SOCK_STREAM, 0);   // get our socket
  server.Bind(13001);  // our port to listen on
  server.Listen(TCPCONN_DEFAULT_BACKLOG);  // start listening
  if (error.Event())
     errx(EX_OSERR, "%s, exiting ...", error.print().c_str());  // ErrorHandler event

  // Enter main event-loop.
  while (1) {
    TCPSession peer( MsgHdr::TYPE_HTTP);  // flow with HTTP framing expected
    server.Accept(&peer);  // accept our connection & populate our flow
    if (error.Event()) {
      warnx("Unable to accept TCP connection: %s.", error.print().c_str());
      error.clear();
      continue;
    }

    // Get data until our message is complete.
    while (!peer.IsIncomingMsgComplete()) {
     bool eof = false;
     ssize_t bytes_read =  peer.Read(&eof);  // slurp up what's on the socket
     bool hdr_ready = peer.InitIncomingMsg();  // grab the message header
     if (error.Event()) {
       warnx("Unable to read data on TCP connection: %s.", error.print().c_str());
       error.clear();
       peer.close();
       break;
    }
  }
 
  // Make sure the header is ready and we have all the expected data ...
   if (peer.IsIncomingMsgComplete()) {
     HTTPFraming http_hdr = peer.rhdr().http_hdr();  // get the message header
     string msg_body;
     msg_body.assign(peer.buf(), peer.rhdr().body_len());  // get the message body

     // Process message-body based on HTTP method & content-type.
     if (http_hdr.method() == HTTPFraming::GET) {
       printf("Received message: %s.\n", msg_body.c_str());
     }       
   }
 }

 exit(0);
}
