// Copyright © 2009, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _IPCOMM_H_
#define _IPCOMM_H_

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>	// requires sys/types.h & includes netinet6/in6.h

#include <arpa/inet.h>	// requires netinet/in.h

#include <string>
#include <list>
using namespace std;

#include "Descriptor.h"


#define IPCOMM_VERSION_MAJOR 1
#define IPCOMM_VERSION_MINOR 0

#define IPCOMM_PORT_NULL 0      // signify that the port is not set

#define IPCOMM_DNS_RETRY_CNT 3  // number of times to try to resolve

#define IP_HDR_LEN 20

// Non-class specific utilities.

/** Routine to calculate the chksum of a payload.
 *
 *  The chksum is calculated using the algorithm in RFC1071.
 *
 */
uint16_t calculate_cksum(const unsigned char* buf, const size_t buf_len);

/** Routine to populate an sockaddr_in with a specific hostname.
 *
 *  TODO(aka) I think this rouitne is deprecated.
 */
void convert_hostname(const char* host, struct sockaddr_in& peer);

/** Routine to resolve a hostname (i.e., return the IP address of a
 *  hostname).
 *
 *  Note, this routine can *block* in gethostbyname(3).
 *
 *  TODO(aka) I think this rouitne is deprecated, as well.
 */
const char* get_reverse_dns(const char* host);


/**
 * Class for controlling IP communications.
 *
 * The IPComm class contains the struct sockaddr_in or struct
 * sockaddr_in6 (for IPv6) and the socket (as a Descriptor object),
 * which are used in IP communications.  The class can be used by
 * itself, or as the base class to higher-layer protocols, e.g.,
 * TCPConn or UDPConn.  Note, all calls are IPv6 safe, i.e., we use
 * getaddrinfo(3) and getnameinfo(3) as opposed to gethostbyname(3).
 *
 * RCSID: $Id: IPComm.h,v 1.2 2012/05/10 17:50:46 akadams Exp $
 *
 * @see Descriptor
 * @see TCPConn
 * @see UDPConn
 * @author Andrew K. Adams <akadams@psc.edu>
 */
class IPComm {
 public:

  /** Constructor.
   *
   */
  IPComm(void);

  /** Constructor.
   *
   *  @param address_family an int specifying the type of IP socket
   */
  explicit IPComm(const int address_family);

  /** Destructor.
   *
   */
  virtual ~IPComm(void);

  /** Copy Constructor.
   *
   *  The copy constructor is needed for class objects used within the STL.
   */
  IPComm(const IPComm& src);

  /** Assignment Operator.
   *
   *  The assignment operator is needed for class objects used within the STL.
   */
  IPComm& operator =(const IPComm& src);

  /** Equality Operator.
   *
   *  The equality operator is needed for *some* data methods for
   *  class objects used within the STL.
   */
  int operator ==(const IPComm& other) const;

  // Accessors.
  int address_family(void) const { return address_family_; }
  list<string> dns_names(void) const { return dns_names_; }

  /** Routine to return the socket file descriptor.
   *
   *  The socket is a Descriptor class. Note, normally this routine
   *  (and its setter version) would be called descriptor() (and
   *  set_descriptor()), not fd() (and set_fd()).  However, if we did
   *  that, then we'd have to return a Descriptor() object ... where, what we
   *  really want is just the *file desscriptor*.
   *
   *  @see Descriptor
   *  @return an int which is the file descriptor
   */
  int fd(void) const { return descriptor_->fd_; }

  /** Routine to return the sin_port within the sockaddr_ union.
   *
   *  The port is stored in the in_port_t sin_port field in either the
   *  sockaddr_in or sockaddr_in6 structs within the union sockaddr_.
   *  Note, technically, the Internet Protocol (IP) does not *know*
   *  about *ports*, as ports are defined in layer 4.  However, as
   *  socket-based libraries store ports within the sockaddr structs,
   *  we are providing the getter and setter functions for ports
   *  within this class.
   *
   *  @return an in_port_t of the port value within union sockaddr_.
   */
  in_port_t port(void) const;

  /** Routine to return the dotted quad for v4 or dotted whatever for v6.
   *
   *  This routine attempts to return the *numeric* IP Address of an
   *  IPComm object by using inet_ntop(), which uses the address family.
   *
   *  @return a string representation of the IP address
   */
  string ip_address(void) const;

  /** Routine to (attempt) to return the fully qualified DNS name.
   *
   *  If we have a value in dns_names_, we return it, else we call
   *  IPComm::ip_address().
   *
   *  @return a string representation of the hostname (or ip address)
   */
  string hostname(void) const;

  // Mutators.
  void clear(void);

  /** Routine to set the socket file descriptor.
   *
   *  @param fd int specifying the new socket file descriptor.
   */
  void set_fd(const int fd);

  /** Routine to set the address family of the object.
   *
   *  @param address_family an int specifying the address family.
   */
  void set_address_family(const int address_family);

  /** Routine to set the port of an IPComm object.
   *
   *  The port is stored in the in_port_t sin_port field within either
   *  the sockaddr_in or sockaddr_in6 structs (in the sockaddr_
   *  union).  Note, see the getter function, port(), above.
   *
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param port an in_port_t specifying the port to use
   */
  void set_port(const in_port_t port);

  /** Routine to mark the socket as BLOCKING.
   *
   *  This routine can only be called *prior* to calling the Socket()
   *  member function.
   */
  void set_blocking(void) { blocking_flag_ = IPComm::BLOCKING; }

  /** Routine to mark the socket as NONBLOCKING.
   *
   *  This routine can only be called *prior* to calling the Socket()
   *  member function.
   */
  void set_nonblocking(void) { blocking_flag_ = IPComm::NON_BLOCKING; }

  /** Routine to mark the socket as staying *open* after an exec(3).
   *
   *  This routine can only be called *prior* to calling the Socket()
   *  member function.
   */
  void set_open_on_exec(void) { exec_flag_ = IPComm::OPEN_ON_EXEC; }

  /** Routine to mark the socket as *closing* after an exec(3).
   *
   *  This routine can only be called *prior* to calling the Socket()
   *  member function.
   */
  void set_close_on_exec(void) { exec_flag_ = IPComm::CLOSE_ON_EXEC; }

  /** Routine to mark the socket as BLOCKING.
   *
   *  This routine can only be called *after* calling the Socket()
   *  member function.
   */
  void set_socket_blocking(void);

  /** Routine to mark the socket as NONBLOCKING.
   *
   *  This routine can only be called *after* calling the Socket()
   *  member function.
   */
  void set_socket_nonblocking(void);

  /** Routine to mark the socket as staying *open* after an exec(3).
   *
   *  This routine can only be called *after* calling the Socket()
   *  member function.
   */
  void set_socket_open_on_exec(void);

  /** Routine to mark the socket as *closing* after an exec(3).
   *
   *  This routine can only be called *after* calling the Socket()
   *  member function.
   */
  void set_socket_close_on_exec(void);

  // Network manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  const string print(void) const;

  /** Routine to copy (or clone) an IPComm object.
   *
   *  As copy constructors are usually frowned apon (except when
   *  needed for the STL), a Clone() method is provided.
   *
   *  @param src the source IPComm to build our object from.
   */
  void Clone(const IPComm& src);

  /** Routine to initialize an IPComm object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed. This entails installing an Internet address in our
   *  sockaddr_ union, i.e., (struct sockaddr_in[6])in[6]_.(struct
   *  in[6]_addr)sin[6]_addr.(in_addr_t or uint8_t[16])s[6]_addr
   *  value, as well as (attempting) to resolve the Internet address
   *  via IPComm:ResolveDNSName() to populate our hostname(s) cache
   *  (dns_names_).
   *
   *  Note, this routine can *block* in getaddrinfo(3).  Additionally,
   *  it can set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler 
   *  @param host is a char* specifiying the Internet address to use
   *  for this peer
   *  @param address_family is an int specifying the address family
   *  @param retry_cnt controls address resolution attempts
   */
  void Init(const char* host, const int address_family, int retry_cnt);

  /** Routine to initialize an IPComm object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed. This entails installing the wildcard Internet
   *  address (either INADDR_ANY or in6addr_any) depending on what
   *  address family is passed in.
   *
   *  This is a convenience routine for building servers.  That is,
   *  servers usually bind() to a wildcard address (e.g., INADDR_ANY
   *  or in6addr_any, this routine generates the wildcard address for
   *  the specified address family.
   *
   *  Note, this routine can set an ErrorHandler event if it
   *  encounters an unrecoverable error.  Additionally, if you don't
   *  want your IPv6 socket to also bind to all local IPv4 interfaces,
   *  then you must also change the socket option, e.g.,
   *  IPComm::Setsockopt(IPPROTO_IPV6, IPV6_V6ONLY, &v6_only,
   *  sizeof(v6_only)), where v6_only is an int set to "1".
   *
   *  @see ErrorHandler 
   *  @see IPComm::Setsockopt()
   *  @param address_family is an int specifying the address family
   */
  void InitServer(const int address_family);

  /** Routine to request a socket from the kernel.
   *
   *  We simply call socket(2).  Note, this routine will set an
   *  ErrorHandler event if it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param domain an int specifying the communication domain
   *  @param type an int specifying communication semantics
   *  @param protocol an int specifying the communication protocol
   */
  void Socket(const int domain, const int type, const int protocol);
  
  /** Routine to retrieve the status of a specific option on a socket.
   *
   *  This routine calls getsockopt(2) on our descriptor_ data member.
   *  As a convenience, the option_value parameter will be converted
   *  to an int (as most socket-level options use an int), and will be
   *  returned from the routine.  Note, this routine will set an
   *  ErrorHandler event if it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param level an int specifying the protocol level for the option
   *  @param option_name an int specifying the option to be queried
   *  @param option_value a value-result buffer to hold option's status
   *  @param option_len a socklen_t* to specify the length of option_value
   *  @return the status of the requested option as an int or -1 on error
   */
  int Getsockopt(int level, int option_name,
                 void* option_value, socklen_t* option_len) const;

  /** Routine to set a specific option on a socket.
   *
   *  We simply call setsockopt(2) on our descriptor_ data member.
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @bug TODO(aka) This routine does not properly handle option_len
   *  @param level an int specifying the protocol level for the option
   *  @param option_name an int specifying the option to be queried
   *  @param option_value a buffer holding the option's status
   *  @param option_len a socklen_t specifying the length of option_value
   */
  void Setsockopt(int level, int option_name,
                  const void* option_value, socklen_t option_len);

  /** Routine to receive a message on a socket.
   *
   *  We simply call recvfrom(2) on our descriptor_ data member.
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param buf a char* to hold the data on the socket
   *  @param len a size_t to specifying the size of buf
   *  @param address a value-result struct sockaddr* for source's information
   *  @param address_len a socklen_t* specifying size of address
   */
  ssize_t Recvfrom(char* buf, const size_t len, 
                   struct sockaddr* address, socklen_t* address_len);

  /** Routine to send a message on a socket.
   *
   *  This routine calls sendto(2) on our descriptor_ data member.
   *  If, however, dest_addr is non-NULL, then we will call sendto(2)
   *  using dest_addr instead of our IPComm object as the destination.
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param buf a char* holds the data to be sent on the socket
   *  @param len a size_t specifies the size of buf
   *  @param dest_addr a struct sockaddr* specifying for destination's address
   *  @param dest_len a socklen_t specifying size of dest_addr
   *  @return a ssize_t specifying the number of bytes sent
   */
  ssize_t Sendto(const char* buf, const size_t len, 
                 const struct sockaddr* dest_addr, const socklen_t dest_len);

  /** Routine to receive a message on a socket.
   *
   *  We simply call recvfrom(2) on our descriptor_ data member.
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param msg a struct msghdr* to hold the incoming message
   *  @param flags an int controlling the behavior of recvmsg(2)
   *  @return a ssize_t specifying the number of bytes read
   */
  ssize_t Recvmsg(struct msghdr* msg, int flags);

  /** Routine to send a message on a socket.
   *
   *  We simply call sendmsg(2) on our descriptor_ data member.
   *  Note, this routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   *  @param msg a struct msghdr* holding the data to be sent on the socket
   *  @param flags an int controlling the behavior of sendmsg(2)
   *  @return a ssize_t specifying the number of bytes sent
   */
  ssize_t Sendmsg(struct msghdr* msg, int flags);

  /** Routine to close a socket.
   *
   *  We simply call close(2) on our descriptor_ data member.
   */
  void Close(void);

  // Boolean checks.

  /** Check to see if our socket is BLOCKING (as opposed to NON-BLOCKING).
   *
   */
  bool IsBlocking(void) const { 
    return  (blocking_flag_ == BLOCKING) ? true : false; }

  /** Check to see if our socket should be kept OPEN after an exec(3) call.
   *
   */
  bool IsOpenOnExec(void) const { 
    return (exec_flag_ == OPEN_ON_EXEC) ? true : false; }

  /** Check to see if we cached (any) DNS names of our object (sockaddr).
   *
   */
  bool IsHostResolved(void) const { 
    return (dns_names_.size() != 0) ? true : false; }

  /** Check to see if our socket has been issued by kernel.
   *
   */
  virtual bool IsConnected(void) const { 
    return (descriptor_->fd_ != DESCRIPTOR_NULL) ? true : false; }

  // Flags.
  enum { BLOCKING, NON_BLOCKING };	// socket flags
  enum { OPEN_ON_EXEC, CLOSE_ON_EXEC };	// socket behavior on exec()

 protected:

  /** Routine to populate our cache of hostnames (dns_names_).
   *
   *  We attempt to resolve our data member (union sockaddr_), and if
   *  successful, populate our hostnames cache (dns_names_) by using
   *  getnameinfo(3).  Note, this routine is called internally from
   *  multiple IPComm methods.  Additionally, this routine sets an
   *  ErrorHandler event if an unrecoverable error is encountered and
   *  clears dns_names_.
   *
   *  @see ErrorHandler
   */
  void ResolveDNSName(int retry_cnt);

  // Data members.
  int blocking_flag_;		 // blocking or non-blocking networking I/O
  int exec_flag_;	         // close or leave socket open after exec(3)

  int address_family_;           // identify which sockaddr we are using

#if 0
  // TOOD(aka) I probably should be using this "union sockunion" here ...
  union sockunion {
    struct sockinet {
      u_char si_len;
      u_char si_family;
    } su_si;
    struct sockaddr_in  su_sin;
    struct sockaddr_in6 su_sin6;
  };
#define su_len        su_si.si_len
#define su_family     su_si.si_family
#endif

  union {
    struct sockaddr_in in_;      // half association of IPv4 socket
    struct sockaddr_in6 in6_;    // half association of IPv6 socket
  } sockaddr_;

  list<string> dns_names_;	 // cache for DNS names, once
                                 // resolved: note, since we use
                                 // getnameinfo(3) instead of
                                 // gethostbyaddr(3), we no longer get
                                 // a list of hostnames ...
  Descriptor* descriptor_;	 // socket file descriptor

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.
};


#endif  /* #ifndef _IPCOMM_H_ */
