// Copyright © 2009, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _TCPCONN_H_
#define _TCPCONN_H_

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>	// requires sys/types.h

#include <arpa/inet.h>	// requires netinet/in.h

#include <string>
using namespace std;

#include "ErrorHandler.h"
#include "IPComm.h"


#define TCPCONN_VERSION_MAJOR 1
#define TCPCONN_VERSION_MINOR 0

#define TCPCONN_DEFAULT_BACKLOG 128    // queue for listen()
#define TCPCONN_DEFAULT_NET_BUF 65536  // max size of pakcet?

// Non-class specific utilities.


/**
 * Class for controlling TCP/IP connections.
 *
 * The TCPConn class is derived from the IPComm class.  It only adds
 * one additional data member (a flag to show if both sides of the
 * connection are in a *connected* state), but it provides several
 * methods that are used within TCP connectoin-oriented protocol,
 * e.g., bind(), listen(), accept(), and connect().  This class can be
 * used by itself, or as the base class to higher-layer abstractions,
 * e.g., TCPSession.
 *
 * RCSID: $Id: TCPConn.h,v 1.2 2012/03/19 15:15:34 akadams Exp $
 *
 * @see Descriptor
 * @see IPComm
 * @see TCPSession
 * @author Andrew K. Adams <akadams@psc.edu>
 */
class TCPConn : public IPComm {
 public:

  /** Constructor.
   *
   */
  TCPConn(void);

  /** Destructor.
   *
   */
  virtual ~TCPConn(void);

  /** Copy Constructor.
   *
   *  The copy constructor is needed for class objects used within the STL.
   */
  TCPConn(const TCPConn& src);

  /** Assignment Operator.
   *
   *  The assignment operator is needed for class objects used within the STL.
   */
  TCPConn& operator =(const TCPConn& src);

  /** Equality Operator.
   *
   *  The equality operator is needed for *some* data methods for
   *  class objects used within the STL.
   */
  int operator ==(const TCPConn& other) const;

  // Accessors.

  // Mutators.
  void set_connected(const bool connected);
  void clear(void);

  // Network manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** Routine to *print* a TCPConn object as a two-tuple (ip:port).
   *
   */ 
  string print_2tuple(void) const;

  /** Routine to *print* a TCPConn object as a three-tuple (proto:ip:port).
   *
   */ 
  string print_3tuple(void) const;

  /** Routine to copy (or clone) a TCPConn object.
   *
   *  As copy constructors are usually frowned apon (except when
   *  needed for the STL), a Clone() method is provided.
   *
   *  @param src the source IPComm to build our object from.
   */
  void Clone(const TCPConn& src);

  /** Routine to initialize an IPComm object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed. This simply entails calling IPComm:Init() to
   *  install an Internet address in our sockaddr_ union, and
   *  associating this TCPConn object to a TCPContext object.
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
  void Init(const char* host, const int address_family, int retry_cnt);

  /** Routine to initialize an IPComm object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed. This simply entails calling IPComm:Init() to
   *  install the wildcard Internet address (either INADDR_ANY or
   *  in6addr_any) depending on what address family is passed in.
   *
   *  Note, this routine can set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler 
   *  @see IPComm
   *  @see IPComm::Setsockopt()
   *  @param address_family is an int specifying the address family
   */
  void InitServer(const int address_family);

  /** Routine to issue a connect(2) to the destination within our object.
   *
   *  This routine attempts to connect(2) *to* the destinatino
   *  specified in the information within this TCPConn object, i.e.,
   *  the information stored within the IPCommm base class.  Note,
   *  this routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler
   */
  void Connect(void);

  /** Routine to issue a connect(2) to the destination specified.
   *
   *  A convenience routine, this routine initialize our object with
   *  the parameters passed in, and then calls TCPConn::Connect() to
   *  attempt to connect(2) using the newly initialized object.  Note,
   *  this routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  TODO(aka) This routine is deprecated.
   *
   *  @see ErrorHandler
   *  @param host a char* specifiying the IP address or DNS name of target
   *  @param port a in_port_t specifying the service to connect to
   *  @param address_family a int specifying the address family to use
   */
  void Connect(const char* host, const in_port_t port, 
               const int address_family);

  /** Routine to assign our local protocol address to our file descriptor.
   *
   *  This routine uses bind(2) to associate the sockaddr_ wtihin our
   *  object to the socket (Descriptor) in our object.  Note, this
   *  routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  TODO(aka) As UDP can use bind(2), we should probably move this
   *  and the the following Bind(in_port_t) to IPComm!
   *
   *  @see ErrorHandler
   */
  void Bind(void);

  /** Routine to assign our local protocol address to our socket.
   *
   *  This routine uses bind(2) to associate the sockaddr_ wtihin our
   *  object to the socket (Descriptor) in our object.  The port
   *  specified as a paramter is first set in our sockaddr_ prior to
   *  calling bind(2). Note, this routine will set an
   *  ErrorHandler event if it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param port an in_port_t specifying the service (local-process) to use
   */
  void Bind(const in_port_t port);

  /** Routine to signify that a socket is willing to receive connections.
   *
   *  This routine uses listen(2) to signify that our object is
   *  willing to receive connections.  The amount of connections that
   *  the kernel will queue before dropping any can be set with the
   *  parameter backlog. Note, this routine will set an ErrorHandler
   *  event if it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param backlog an int signifying how many connections to queue
   */
  void Listen(const int backlog);

  /** Routine to create a new TCPConn object from a completed connection.
   *
   *  This routine uses accept(2) to initialize a TCPConn object from
   *  the remote socket (half-association) that most recently
   *  completed a connection with us.  Additionally, a new socket
   *  Descriptor is assigned to the TCPConn objecct.  Note, this
   *  routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  TODO(aka) Can an IPv6 *listening* socket accept(2) an IPv4
   *  connection?  If so, then we'll need to change this routine to
   *  check for this!
   *
   *  @see ErrorHandler
   *  @param peer a TCPConn* to hold the new socket
   */
  void Accept(TCPConn* peer) const;

  /** Routine to create a new TCPConn object from a completed connection.
   *
   *  This routine uses accept(2) to initialize a TCPConn object from
   *  the remote socket (half-association) that most recently
   *  completed a connection with us.  Additionally, a new socket
   *  Descriptor is assigned to the TCPConn objecct.  This is a
   *  convenience routine that takes advantage of our copy
   *  constructor, i.e., it calls TCPConn::Accept(TCPConn* peer) with
   *  its own local copy of a TCPConn object, and retuns that copy.
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @return a TCPConn that holds the new socket
   */
  TCPConn Accept(void) const;

  /** Routine to close(2) a connected socket.
   *
   *  We simply call IPComm::Close() to get the work done.
   */
  void Close(void);

  /** Routine to get local-address and local-process (port).
   *
   *  This routine retrieves the local-address and local-process
   *  (port) from a connected socket via getsockname(2).  The caller
   *  is responsible for passing in a sockaddr* of sufficent size to
   *  accomadate the sockaddr for the address family of the connected
   *  socket (see IPComm::address_family()). Note, this routine will
   *  set an ErrorHandler event if it encounters an unrecoverable
   *  error.
   *
   *  @see IPComm::address_family()
   *  @see ErrorHandler
   *  @param address a sockaddr* to hold the *local* half-association
   *  @param address_len a socklen_t* specifying the size of address
   */
  void Getsockname(struct sockaddr* address, socklen_t* address_len) const;

  /** Routine to write a chunk of data out our socket.
   *
   *  This routine uses write(2) to write a chunk of data in buf of
   *  size len out our socket. Note, this routine will set an
   *  ErrorHandler event if it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param buf a char* holding the data to be written
   *  @param len a ssize_t showing the amount of data in buf
   *  @return a ssize_t showing the amount of data written
   */
  ssize_t Write(const char* buf, const ssize_t len) const;

  /** Routine to read a chunk of data from our socket.
   *
   *  This routine uses read(2) to read a chunk of data to buf of size
   *  len from our socket. If end-of-file is read, the flag eof is set
   *  to true.  Note, this routine will set an ErrorHandler event if
   *  it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param len a ssize_t showing the amount of data in buf
   *  @param buf a char* to hold the data read
   *  @param eof a bool* signifying that EOF was read
   *  @return a ssize_t showing the amount of data read
   */
  ssize_t Read(const ssize_t len, char* buf, bool* eof) const;

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

  // Boolean checks.

  /** Routine to report if a socket is in the *connected* state.
   *
   */
  bool IsConnected(void) const { return connected_; }

  /** Routine to report if a socket is in the *listening* state.
   *
   */
  bool IsListening(void) const { return listening_; }

  /** Routine to report if our object is the same as another.
   *
   *  This routine simply uses TCPConn::operator==(), as we had to
   *  write that for the STL anyways.
   */
  bool Equals(const TCPConn& other) const;

  // Flags.

 protected:
  // Data members.
  bool connected_;       // flag to show that we have an active connection
  bool listening_;       // flag to show that we are in a *listen* state

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.

  // Since we're dervived from IPComm, we need to prevent someone from
  // accidentally using the connection-less I/O routines.

  ssize_t Recvfrom(char* buf, const size_t len, 
                   struct sockaddr* address, socklen_t* address_len);
  ssize_t Sendto(const char* buf, const size_t len, 
                 const struct sockaddr* dest_addr, const socklen_t dest_len);
  ssize_t Recvmsg(struct msghdr* msg, int flags);
  ssize_t Sendmsg(struct msghdr* msg, int flags);
};


#endif  /* #ifndef _TCPCONN_H_ */
