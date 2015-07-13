// Copyright © 2009, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _SSLCONN_H_
#define _SSLCONN_H_

#include <sys/types.h>

#include <openssl/ssl.h>

#include <string>
using namespace std;

#include "ErrorHandler.h"
#include "TCPConn.h"
#include "SSLContext.h"


#define SSLCONN_VERSION_MAJOR 1
#define SSLCONN_VERSION_MINOR 0


// Non-class specific utilities.


/**
 * Class for controlling SSL/TLS encryption over TCP/IP connections.
 *
 * The SSLConn class is derived from the TCPConn class, which itself
 * is dervived from the IPComm class.  It only adds three additional
 * data member: the OpenSSL object, the peer certificate (if we
 * received one), and a flag to show what state we are in (useful for
 * terminating the connection).  Several of TCP connectoin-oriented
 * functions, e.g., bind(), listen(), accept(), connect(), read() and
 * write() are overwritten.  This class can be used by itself, or as
 * the base class to higher-layer abstractions, e.g., SSLSession.
 *
 * RCSID: $Id: SSLConn.h,v 1.2 2012/03/19 15:15:34 akadams Exp $
 *
 * @see Descriptor
 * @see IPComm
 * @see TCPConn
 * @see SSLSession
 * @author Andrew K. Adams <akadams@psc.edu>
 */
class SSLConn : public TCPConn {
 public:

  /** Constructor.
   *
   */
  SSLConn(void);

  /** Destructor.
   *
   */
  virtual ~SSLConn(void);

  /** Copy Constructor.
   *
   *  The copy constructor is needed for class objects used within the STL.
   */
  SSLConn(const SSLConn& src);

  /** Assignment Operator.
   *
   *  The assignment operator is needed for class objects used within the STL.
   */
  SSLConn& operator =(const SSLConn& src);

  /** Equality Operator.
   *
   *  The equality operator is needed for *some* data methods for
   *  class objects used within the STL.
   */
  int operator ==(const SSLConn& other) const;

  // Accessors.
  const SSLContext* ctx(void) const { return ctx_; }
  const SSL* ssl(void) const { return ssl_; }
  const X509* peer_certificate(void) const { return peer_certificate_; }

  // Mutators.
  void clear(void);

  // Network manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** Routine to initialize an IPComm object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed. This simply entails calling IPComm:Init() to
   *  install an Internet address in our sockaddr_ union, and
   *  associating this SSLConn object to a SSLContext object.
   *
   *  Note, this routine can set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler 
   *  @see IPComm
   *  @param host is a char* specifiying the Internet address to use
   *  for this peer
   *  @param address_family is an int specifying the address family
   *  @param retry_cnt controls address resolution attempts
   */
  void Init(const SSLContext& ctx, const char* host, const int address_family, int retry_cnt);

  /** Routine to initialize an IPComm object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed. This simply entails calling IPComm:Init() to
   *  install the wildcard Internet address (either INADDR_ANY or
   *  in6addr_any) depending on what address family is passed in, and
   *  associating this SSLConn object to a SSLContext object.
   *
   *  Note, this routine can set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler 
   *  @see IPComm
   *  @see IPComm::Setsockopt()
   *  @param address_family is an int specifying the address family
   */
  void InitServer(const SSLContext& ctx, const int address_family);

  /** Routine to request a socket from the kernel.
   *
   *  We simply call IPComm::Socket(), however, after getting a valid
   *  file descriptor in Descriptor, we generate a SSL* object.  This
   *  allows us to reference count both the file decriptor and SSL*
   *  object using Descriptor().
   *
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @see IPComm
   *  @param domain an int specifying the communication domain
   *  @param type an int specifying communication semantics
   *  @param protocol an int specifying the communication protocol
   */
  void Socket(const int domain, const int type, const int protocol);

  /** Routine to issue a connect(2) to the destination within our object.
   *
   *  This routine attempts to connect(2) *to* the destinatino
   *  specified in the information within this SSLConn object, i.e.,
   *  the information stored within the TCPConn & IPCommm base
   *  classes.  Note, this routine will set an ErrorHandler event if
   *  it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @see TCPConn
   */
  void Connect(void);

  /** Routine to create a new SSLConn object from a completed connection.
   *
   *  This routine uses accept(2) to initialize a SSLConn object from
   *  the remote socket (half-association) that most recently
   *  completed a connection with us.  Additionally, a new socket
   *  Descriptor is assigned to the SSLConn objecct.
   *
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  TODO(aka) Can an IPv6 *listening* socket accept(2) an IPv4
   *  connection?  If so, then we'll need to change this routine to
   *  check for this!
   *
   *  @see ErrorHandler
   *  @param peer a SSLConn* to hold the new socket
   */
  void Accept(SSLConn* peer) const;

  /** Routine to create a new SSLConn object from a completed connection.
   *
   *  This routine uses accept(2) to initialize a SSLConn object from
   *  the remote socket (half-association) that most recently
   *  completed a connection with us.  Additionally, a new socket
   *  Descriptor is assigned to the SSLConn objecct.  This is a
   *  convenience routine that takes advantage of our copy
   *  constructor, i.e., it calls SSLConn::Accept(SSLConn* peer) with
   *  its own local copy of a SSLConn object, and retuns that copy.
   *
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @return a SSLConn that holds the new socket
   */
  SSLConn Accept(void) const;

  /** Routine to gracefully close an SSL connection.
   *
   * We initiate a SSL close by calling this routine, i.e., send our
   * *close notify*, or upon receiving a *close notify*, we call this
   * routine to finish the bi-directional close.  If we do not want to
   * bother with a graceful SSL close, we can specify the
   * uni-directional flag, which will mark the SSL connection as
   * closed after sending our close notify regardless if we've
   * received one from our peer.
   *
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param uni_directional true if we do not care about receiving peer's close notify
   */
  void Shutdown(const int unidirectional);

  /** Routine to write a chunk of data out our socket.
   *
   *  This routine uses write(2) to write a chunk of data in buf of
   *  size len out our socket.
   *
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.  Additionally, we can't make
   *  this member function const, as we may need to call Shutdown()
   *  (which calls IPComm::Close(), which is not a const member
   *  function).
   *
   *  @see ErrorHandler
   *  @param buf a char* holding the data to be written
   *  @param len a ssize_t showing the amount of data in buf
   *  @return a ssize_t showing the amount of data written
   */
  ssize_t Write(const char* buf, const ssize_t len);

  /** Routine to read a chunk of data from our socket.
   *
   *  This routine uses read(2) to read a chunk of data to buf of size
   *  len from our socket. If end-of-file is read, the flag eof is set
   *  to true.  Note, this routine will set an ErrorHandler event if
   *  it encounters an unrecoverable error.  Also, we can't make this
   *  const, as Shutdown() (which calls IPComm::Close(), which is not
   *  a const member function).
   *
   *  @see ErrorHandler
   *  @param len a ssize_t showing the amount of data in buf
   *  @param buf a char* to hold the data read
   *  @param eof a bool* signifying that EOF was read
   *  @return a ssize_t showing the amount of data read
   */
  ssize_t Read(const ssize_t len, char* buf, bool* eof);

#if 0  // TODO(aka)
  /** Routine to read a (perhaps multiple) chunk(s) of data from our socket.
   *
   *  This routine uses read(2) to read a chunk of data to buf of size
   *  len from our socket. If end-of-file is read, the flag eof is set
   *  to true.  A convenience routine, this routine reads from the
   *  socket as long as there is data and our buffer has room, i.e.,
   *  it will issue read(2) multiple times if necessary. Note, this
   *  routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param len a ssize_t showing the amount of data in buf
   *  @param buf a char* to hold the data read
   *  @param eof a bool* signifying that EOF was read
   *  @return a ssize_t showing the amount of data read
   */
  ssize_t ReadExhaustive(const ssize_t len, char* buf, bool* eof) const;

  /** Routine to read a (perhaps multiple) chunk(s) of data from our socket.
   *
   *  This routine uses read(2) to read a chunk of data to buf of size
   *  len from our socket. If end-of-file is read, the flag eof is set
   *  to true.  A convenience routine, this routine reads from the
   *  socket until either the buffer size is reached or the framing
   *  character (or '\0') is encountered. Note, this routine will set
   *  an ErrorHandler event if it encounters an unrecoverable error.
   *
   *  TODO(aka) This should probably be called ReadToken(), hmm ...
   *
   *  @see ErrorHandler
   *  @param framing a char signifying the character that delimits reads
   *  @param len a ssize_t showing the amount of data in buf
   *  @param buf a char* to hold the data read
   *  @param eof a bool* signifying that EOF was read
   *  @return a ssize_t showing the amount of data read
   */
  ssize_t ReadLine(const char framing, const ssize_t len, char* buf,
                   bool* eof) const;
#endif

  // Boolean checks.
  const int IsShutdownInitiated(void) const;
  const int IsShutdownComplete(void) const;

#if 0  // XXX
  /** Routine to report if our object is the same as another.
   *
   *  This routine simply uses SSLConn::operator==(), as we had to
   *  write that for the STL anyways.
   */
  bool Equals(const SSLConn& other) const;
#endif

  // Flags.

 protected:
  // Data members.
  const SSLContext* ctx_;   // pointer to the non-editable OpenSSL *context* that we are using
  SSL* ssl_;                // OpenSSL *connection* object.  Note, this
                            // object needs referenced counted,
                            // however, since SSL objects are
                            // associated with open sockets, we can
                            // use the Descriptor class reference
                            // count.  We just have to (i) make the
                            // SSLConn class a friend in Descriptor,
                            // and (ii) not call SSL_new() until we
                            // have a Descriptor object (which means
                            // waiting until Connect() & Accept()).

  X509* peer_certificate_;  // certificate of peer

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.

  // Since we're dervied from TCPConn, we need to prevent someone from
  // accidentally using non-encrypted I/O.

  ssize_t ReadExhaustive(const ssize_t len, char* buf, bool* eof) const;
  ssize_t ReadLine(const char framing, const ssize_t len, char* buf,
                   bool* eof) const;
};


#endif  /* #ifndef _SSLCONN_H_ */
