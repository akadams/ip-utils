// $Id: IPComm.cc,v 1.5 2013/09/13 14:56:38 akadams Exp $ //

// Copyright (c) 1998, see the file 'COPYRIGHT.h' for any restrictions.

#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/types.h>

#include <err.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include "ErrorHandler.h"
#include "Logger.h"
#include "IPComm.h"

#define DEBUG_CLASS 0

#define SCRATCH_BUF_SIZE 1024

// TODO(aka) See if we can replace any of the warnx() calls regarding
// unknown family type with ErrorHandler events.  Note, if we use
// IPComm::Init(), then we'd already set an ErrorHandler event for the
// unknown address type, so all the others are probably redundant and
// can be removed (assuming IPComm::Init() is the only way to
// initialize a socket).

// Non-class specific utility functions.

// Routine to calculate the ones complement of the sum of each 16 bit
// word contained in the buffer.
uint16_t calculate_cksum(const unsigned char* buf, const size_t buf_len) {
  // Note, this algroithm was taken from RFC 1071.

  uint32_t sum = 0;  // this should probably be a long
  uint16_t* buf_ptr = (uint16_t*)buf;  // for correct pointer arithmetic
  size_t num_left = buf_len;

  // Sum up each 16bit word into our accumulator.
  while (num_left > 1) {
    sum += *buf_ptr++;  // don't forget to dereference
    // *and* increment ptr by 2 bytes
    num_left -= 2;
  }

  // Add the left-over byte if one still remains.
  if (num_left == 1) 
    sum += *(unsigned char*)buf_ptr;  // we only want one byte here
		
  // Fold high-order 16bits of accumulator into lower-order 16bits.
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }

  uint16_t cksum = ~sum;  // one's complement & truncate our accumulator

  return cksum;
}

// Routine to return hostname as an Internet address (in_addr_t)
// within a sockaddr_in struct.
//
// Note, this routine can *block* in gethostbyname(3).
//
// This routine can set an ErrorHandler event.
void convert_hostname(const char* host, struct sockaddr_in& peer) {
  // TODO(aka) I think this routine should be named
  // host_to_sockaddr_t() or gen_sockaddr_t().

  error.Init(EX_SOFTWARE, "IPCommm::convert_hostname(): "
             "TOOD(aka) not implemented yet");
  return;

#if 0
  // Convert the hostname to an Internet address.
  unsigned long tmp_inet_addr = 0;
  if ((tmp_inet_addr = inet_addr(host)) != (unsigned long)-1) {
    // Easy, it *is* dotted decimal.
    peer.sin_addr.s_addr = (in_addr_t)tmp_inet_addr;
  } else {
    // See if we have a valid domain name.

    // TODO(aka) IPv6 needs gethostbyname2().
    int retry_cnt = IPCOMM_DNS_RETRY_CNT;
    struct hostent* hp = NULL;
    while (! (hp = gethostbyname(host)) && h_errno == TRY_AGAIN && retry_cnt) {
      sleep(1);	// sleep & try again
      --retry_cnt;
    }
		
    if (hp) {
      // It resolved, grab the first address in h_addr_list!
      memcpy(&peer.sin_addr, hp->h_addr_list[0], hp->h_length);
    } else {
      // This sucks, it didn't resolve ...
      memset(&peer.sin_addr, 0, sizeof(struct in_addr));
    }
  }
#endif
}

// Routine to resolve a hostname (i.e., return the IP address of a
// hostname).
//
// Note, this routine can *block* in gethostbyname(3).
const char* get_reverse_dns(const char* host) {

  // Note, since hp->h_addr_list[0] is internal storage, it is the
  // responsibility of the calling program to make certain that it
  // sets aside storage for the returned char*, *if* it intends to
  // make multiple back to back calls.

  // First, see if we already have a reverse IP address.
  unsigned long tmp_inet_addr = 0;
  if ((tmp_inet_addr = inet_addr(host)) != (unsigned long)-1) {
    // Cool, it *is* dotted decimal.
    return host;
  } else {
    // See if we have a valid domain name.

    // TODO(aka) IPv6 needs gethostbyname2().
    int retry_cnt = IPCOMM_DNS_RETRY_CNT;
    struct hostent* hp = NULL;
    while (! (hp = gethostbyname(host)) && h_errno == TRY_AGAIN && retry_cnt) {
      sleep(1);	// sleep & try again
      --retry_cnt;
    }
		
    if (hp) {
      // It resolved, grab the first address from h_addr_list
      if (hp->h_addr_list[0]) {
        return inet_ntoa(*((struct in_addr*)hp->h_addr_list[0]));
      }
    }

    return NULL;  // report failure, if we made it here
  }

  // Not Reached!
}

// Constructor & destructor functions.
IPComm::IPComm(void) 
    : dns_names_() {
#if DEBUG_CLASS
  warnx("IPComm::IPComm(void) called.");
#endif

  // Set the flags to defaults.
  blocking_flag_ = BLOCKING;
  exec_flag_ = OPEN_ON_EXEC;
  address_family_ = AF_UNSPEC;

  // Zero out union sockaddr_.
  memset(&sockaddr_, 0, sizeof(sockaddr_));

  descriptor_ = new Descriptor();
}

IPComm::IPComm(const int address_family) 
    : dns_names_() {
#if DEBUG_CLASS
  warnx("IPComm::IPComm(void) called.");
#endif

  // Set the flags to defaults.
  blocking_flag_ = BLOCKING;
  exec_flag_ = OPEN_ON_EXEC;
  address_family_ = address_family;

  // Zero out union sockaddr_.
  memset(&sockaddr_, 0, sizeof(sockaddr_));

  descriptor_ = new Descriptor();
}

IPComm::~IPComm(void) {
#if DEBUG_CLASS
  warnx("IPComm::~IPComm(void) called, cnt: %d, fd: %d.", 
        descriptor_->cnt_, descriptor_->fd_);
#endif
  if (!--descriptor_->cnt_) {
    if (descriptor_->fd_ != DESCRIPTOR_NULL) {
      Close();
    }

    delete descriptor_;
  }
}

// Copy constructor and assignment needed for STL.
IPComm::IPComm(const IPComm& src) 
    : dns_names_(src.dns_names_) {
#if DEBUG_CLASS
  warnx("IPComm::IPComm(const IPComm&) called, src cnt: %d, fd: %d.", 
        src.descriptor_->cnt_, src.descriptor_->fd_);
#endif

  blocking_flag_ = src.blocking_flag_;
  exec_flag_ = src.exec_flag_;
  address_family_ = src.address_family_;

  switch (address_family_) {
    case AF_INET : memcpy(&sockaddr_.in_, (void*)&src.sockaddr_.in_, 
                          sizeof(sockaddr_.in_));
      break;

    case AF_INET6 : memcpy(&sockaddr_.in6_, (void*)&src.sockaddr_.in6_, 
                           sizeof(sockaddr_.in6_));
      break;

    default :
      warnx("IPComm::IPComm(const IPComm& src): unknown address_family: %d.",
            address_family_);
      memcpy(&sockaddr_, (void*)&src.sockaddr_, sizeof(sockaddr_));
  }

  // Since our Descriptor data member is just a pointer, we simply set
  // it to point to the source, then bump its reference count.

  descriptor_ = src.descriptor_;
  descriptor_->cnt_++;
}

IPComm& IPComm::operator =(const IPComm& src) {
#if DEBUG_CLASS
  warnx("IPComm::operator =(const IPComm&) called, "
        "cnt: %d, fd: %d, src cnt: %d, fd: %d.", 
        descriptor_->cnt_, descriptor_->fd_, 
        src.descriptor_->cnt_, src.descriptor_->fd_);
#endif

  blocking_flag_ = src.blocking_flag_;
  exec_flag_ = src.exec_flag_;
  address_family_ = src.address_family_;
  switch (address_family_) {
    case AF_INET : memcpy(&sockaddr_.in_, (void*)&src.sockaddr_.in_, 
                          sizeof(sockaddr_.in_));
      break;

    case AF_INET6 : memcpy(&sockaddr_.in6_, (void*)&src.sockaddr_.in6_, 
                           sizeof(sockaddr_.in6_));
      break;

    default :
      warnx("IPComm::IPComm(const IPComm& src): unknown address_family: %d.",
            address_family_);
      memcpy(&sockaddr_, (void*)&src.sockaddr_, sizeof(sockaddr_));
  }

  dns_names_ = src.dns_names_;

  // If we're about to remove our last instance of the Descriptor we
  // currently point to, clean it up first!

  if (!--descriptor_->cnt_) {
    if (descriptor_->fd_ != DESCRIPTOR_NULL) {
      Close();
    } 
    delete descriptor_;
  }

  // Now, since our Descriptor data member is just a pointer, we can
  // (re-)associate it to source's, then bump its reference count.

  descriptor_ = src.descriptor_;
  descriptor_->cnt_++;

  return *this;
}

// Overloaded operator: equality functions.
//
// TOOD(aka) I wish I knew what damn STL method is requiring us to
// have this damn function.  list::sort() perhaps?
int IPComm::operator ==(const IPComm& other) const {
#if DEBUG_CLASS
  warnx("IPComm::operator ==() called.");
#endif

  // TODO(aka) Change the test to use hostname() and descriptor_->fd_.
  // Or at least check the len of both strings first!

  // If the host & fd match, then return 1, else return 0;
  if (!strncmp(print().c_str(), other.print().c_str(), print().length()))
    return 1;

  return 0;
}

// Accessors.

// Routine to return the *numeric* IP Address of an IPComm object by
// using inet_ntop().
string IPComm::ip_address(void) const { 
  string tmp_str(1024, '\0');  // '\0' so strlen() works
  const char* dst = NULL;
  switch (address_family_) {
    case AF_INET : 
      dst = inet_ntop(address_family_, &sockaddr_.in_.sin_addr, 
                      (char*)tmp_str.c_str(), 1024);
      break;

    case AF_INET6 : 
      dst = inet_ntop(address_family_, &sockaddr_.in6_.sin6_addr, 
                      (char*)tmp_str.c_str(), 1024);
      break;

    default :
      warnx("IPComm::ip_address(): unknown address_family: %d.",
            address_family_);
  }

  if (dst == NULL) {
    warn("IPComm::ip_address(): inet_ntop(%d) failed", address_family_);
    return "NULL";
  }

  return tmp_str;

#if 0  
  // TODO(aka) Deprecated ipv4-only method.
  return inet_ntoa(sock_addr_.sin_addr);  // TODO(aka) Does
                                          // inet_ntoa() set aside
                                          // space for the data?
#endif
}

// Routine to return the DNS name of the IPComm object.  If we don't
// have one (not sure under what cases this would actually be the
// case), we call IPComm::ip_address().
string IPComm::hostname(void) const {
  // If we have one, return the DNS name.
  if (dns_names_.size())
    return dns_names_.front().c_str();

  return ip_address();  // last resort ;-)
}

// Routine to return the port inside our sockaddr_.
in_port_t IPComm::port(void) const {
  in_port_t port = IPCOMM_PORT_NULL;
  switch (address_family_) {
    case AF_INET : 
      port = ntohs(sockaddr_.in_.sin_port);
      break;

    case AF_INET6 : 
      port = ntohs(sockaddr_.in6_.sin6_port);
      break;

    default :
      warnx("IPComm::port(): unknown address_family: %d.",
            address_family_);
  }

  return port;
}

// Mutators.

void IPComm::set_fd(const int fd) { 
  descriptor_->fd_ = fd; 
}

void IPComm::set_address_family(const int address_family) {
  address_family_ = address_family;

  switch (address_family_) {
    case AF_INET :
      sockaddr_.in_.sin_family = address_family_;
      break;

    case AF_INET6 :
      sockaddr_.in6_.sin6_family = address_family_;
      break;

    default :
      warnx("IPComm::set_address_family(): unknown address_family: %d.",
            address_family_);
  }
}

void IPComm::set_port(const in_port_t port) {
  if (address_family_ == AF_UNSPEC) {
    error.Init(EX_SOFTWARE, "IPComm::set_port(): address_family is AF_UNSPEC");
    return;
  }

  switch (address_family_) {
    case AF_INET :
      sockaddr_.in_.sin_port = htons(port);
      break;

    case AF_INET6 :
      sockaddr_.in6_.sin6_port = htons(port);
      break;

    default :
      warnx("IPComm::set_address_family(): unknown address_family: %d.",
            address_family_);
  }
}

// Routine to set an open file descriptor as "blocking".
//
// Note, this routine can set an ErrorHandler event.
void IPComm::set_socket_blocking(void) {
  if (descriptor_->fd_ == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "IPComm::set_socket_blocking(): "
               "descriptor is NULL");
    return;
  }

  // TODO(aka) Perhaps we should have set_blocking_flag, and then two
  // helper functions, SetSocketBlocking() and SetSocketNonBlocking
  // that call set_blocking_flag()?

  // Get socket's current flags ...
  int val = 0;
  if ((val = fcntl(descriptor_->fd_, F_GETFL, 0)) < 0) {
    error.Init(EX_IOERR, "IPComm::set_socket_blocking(): fcntl(F_GETFL): %s",
               strerror(errno));
    return;
  }

  // ... and see if the O_NONBLOCK bit is set.
  int result = val & O_NONBLOCK;
  if (! result) {
    val &= ~O_NONBLOCK;  // disable the non-blocking bit

    if (fcntl(descriptor_->fd_, F_SETFL, val) < 0) {
      error.Init(EX_IOERR, "IPComm::set_socket_blocking(): fcntl(F_SETFL): %s",
                 strerror(errno));
      return;
    }
  }

  blocking_flag_ = BLOCKING;
}

// Routine to set an open file descriptor to 'non-blocking'.
//
// Note, this routine can set an ErrorHandler event.
void IPComm::set_socket_nonblocking(void) {
  if (descriptor_->fd_ == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "IPComm::set_socket_nonblocking(): "
               "descriptor is NULL");
    return;
  }

  // Get socket's current fd flags ...
  int val = 0;
  if ((val = fcntl(descriptor_->fd_, F_GETFL, 0)) < 0) {
    error.Init(EX_IOERR, "IPComm::set_socket_nonblocking(): fcntl(F_GETFL): %s",
               strerror(errno));
    return;
  }

  // ... and see if the O_NONBLOCK bit is set.
  int result = val & O_NONBLOCK;
  if (! result) {
    val |= O_NONBLOCK;

    if (fcntl(descriptor_->fd_, F_SETFL, val) < 0) {
      error.Init(EX_IOERR, "IPComm::set_socket_nonblocking(): fcntl(F_SETFL): %s",
                 strerror(errno));
      return;
    }
  }

  blocking_flag_ = NON_BLOCKING;
}

// Routine to set the F_SETFD flag to 'open-on-exec'.
//
// Note, this routine can set an ErrorHandler event.
void IPComm::set_socket_open_on_exec(void) {
  if (descriptor_->fd_ == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "IPComm::set_socket_open_on_exec(): "
               "descriptor is NULL");
    return;
  }

  // Set the bit.
  if (fcntl(descriptor_->fd_, F_SETFD, 0) == -1) {
    error.Init(EX_IOERR, "IPComm::set_socket_open_on_exec(): "
               "fcntl(%d, F_SETFD, 0): %s", descriptor_->fd_, strerror(errno));
    return;
  }

  exec_flag_ = OPEN_ON_EXEC;
}

// Routine to set the F_SETFD flag to 'close-on-exec'.
//
// Note, this routine can set an ErrorHandler event.
void IPComm::set_socket_close_on_exec(void) {
  if (descriptor_->fd_ == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "IPComm::set_socket_close_on_exec(): "
               "descriptor is NULL");
    return;
  }

  // Set the bit.
  if (fcntl(descriptor_->fd_, F_SETFD, 1) == -1) {
    error.Init(EX_IOERR, "IPComm::set_socket_close_on_exec(): "
               "fcntl(%d, F_SETFD, 1): %s", descriptor_->fd_, strerror(errno));
    return;
  }

  exec_flag_ = CLOSE_ON_EXEC;
}

// Routine to clear the member data, i.e., set them to defaults.
void IPComm::clear(void) {
#if DEBUG_CLASS
  warnx("IPComm::clear(void) called, cnt: %d, fd: %d.", 
        descriptor_->cnt_, descriptor_->fd_);
#endif

  blocking_flag_ = BLOCKING;
  exec_flag_ = OPEN_ON_EXEC;
  address_family_ = AF_UNSPEC;
  dns_names_.clear();
  memset(&sockaddr_, 0, sizeof(sockaddr_));

  // If we're about to remove our last instance of the current
  // Descriptor, clean up the Descriptor object.

  if (!--descriptor_->cnt_) {
    if (descriptor_->fd_ != DESCRIPTOR_NULL) {
      Close();
    } 
    delete descriptor_;
  }

  descriptor_ = new Descriptor();
}

// Network manipulation functions.

// Routine to *pretty* print object.
const string IPComm::print(void) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // so strlen() works

  // Print retuns the host & socket.
  snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s:%d", 
           hostname().c_str(), descriptor_->fd_);

  return tmp_str;
}

// Copy method.
void IPComm::Clone(const IPComm& src) {
  IPComm::operator =(src);  // use our assignment operator, since we have one
}

// Routine to initialize our IPComm object.  This entails installing
// an Internet address in our sockaddr_ union, i.e., (struct
// sockaddr_in[6])in[6]_.(struct in[6]_addr)sin[6]_addr.(in_addr_t or
// uint8_t[16])s[6]_addr value, as well as (attempting) to resolve the
// Internet address via IPComm:ResolveDNSName() to populate our
// hostname(s) cache (dns_names_).
//
// Note, this routine can *block* in getaddrinfo(3) and can set an
// ErrorHandler event.
void IPComm::Init(const char* host, const int address_family, 
                  int retry_cnt) {

  // TOOD(aka) This routine would probably be more efficient if, when
  // host is *not* a dotted-numeric (either v4 or v6), then we
  // woulnd't have to call ResolveDNSName (assuming getaddrinfo() was
  // able to produce a sockaddr for host).  That is, we simply do a
  // dns_names_.push_back(host) instead of calling ResolveDNSNames().
  // However, to do this though, we'd need to use inet_pton() (which
  // would require specifiying the address family up front) -- we
  // could see if inet_pton() call succeeds, meaning we have a
  // dotted-numeric, or if it fails, we have a DNS name (most likely).
  // On the other hand, if host *is* a dotted-numeric, calling
  // ResolveDNSname() (which calls getnameinfo()) *after* we call
  // getaddrinfo() in here may not be all that bad, as we can probably
  // assume that any resolving in getaddrinfo() would be cached
  // locally for getnameinfo() to slurp up, no?

  // Get the sockaddr_in[6] for the requested address.
  struct addrinfo* addresses;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  if (address_family != AF_UNSPEC)
    hints.ai_family = PF_UNSPEC;  // set-up hints structure
  else
    hints.ai_family = address_family;  // set-up hints structure
  //hints.ai_flags = AI_PASSIVE;  // to get IN_ADDR_ANY & IN6ADDR_ANY_INIT (host must be NULL)

  int ecode = 0;
  while (1) {
    ecode = getaddrinfo(host, NULL, &hints, &addresses);
    if (!ecode || !--retry_cnt || ecode != EAI_AGAIN)
      break;  // either it worked or we're out of time
    else {
      _LOGGER(LOG_DEBUG, "IPComm::Init() retry_cnt = %d && ecode == %d.\n", 
              retry_cnt, ecode);
      sleep(1);
    }
  }

  // See if we still have a standing error.
  if (ecode) {
    if (ecode == EAI_SYSTEM)
      error.Init(EX_OSERR, "IPComm::Init(): %s", strerror(errno));
    else
      error.Init(EX_OSERR, "IPComm::Init(): %s", gai_strerror(ecode));
    return;
  } 

  // getaddrinfo() worked, get our sockaddr ...
  for (struct addrinfo* address = addresses; address != NULL;
       address = address->ai_next) {
    set_address_family(address->ai_family); 
    
    switch (address_family_) {
      case AF_INET :
        if (address->ai_addrlen != sizeof(struct sockaddr_in)) {
          error.Init(EX_SOFTWARE, "IPComm::Init(): "
                     "ai_addrlen: %ld, does not match struct size: %ld",
                     address->ai_addrlen, sizeof(struct sockaddr_in));
          return;
        }
        memcpy(&sockaddr_.in_, address->ai_addr, address->ai_addrlen);
        break;

      case AF_INET6 :
        if (address->ai_addrlen != sizeof(struct sockaddr_in6)) {
          error.Init(EX_SOFTWARE, "IPComm::Init(): "
                     "ai_addrlen: %ld, does not match struct size: %ld",
                     address->ai_addrlen, sizeof(struct sockaddr_in6));
          return;
        }
        memcpy(&sockaddr_.in6_, address->ai_addr, address->ai_addrlen);
        break;

      default :
        error.Init(EX_SOFTWARE, "IPComm::Init(): "
                   "unknown address family: %d", address_family_);
        return;
    }

    break;  // for now, just take the first sockaddr returned
  }
  freeaddrinfo(addresses);

  // Populate dns_names_.  Note, if host was dotted-numeric, we may be
  // resolving twice (see above note!).

  ResolveDNSName(retry_cnt);  // can set an ErrorHandler event

#if 0
  // TODO(aka) Deprecated IPv4-only method.

  // Convert the hostname to an Internet address.
  unsigned long tmp_inet_addr = 0;
  if ((tmp_inet_addr = inet_addr(host)) != (unsigned long)-1) {
    // Cool, it's dotted decimal, so let's set it, resolve it & 
    // be done with it.

    sock_addr_.sin_addr.s_addr = (in_addr_t)tmp_inet_addr;
    xxx; // can set an ErrorHandler event
    ResolveDNSName(IPCOMM_DNS_RETRY_CNT);  // set 'dns_name_'
  } else {
    // See if we have a valid domain name.
    int retry_cnt = IPCOMM_DNS_RETRY_CNT;

    // TODO(aka) IPv6 needs gethostbyname2().
    struct hostent* hp = NULL;
    while (! (hp = gethostbyname(host)) && 
           h_errno == TRY_AGAIN && retry_cnt) {
      sleep(1);	// sleep & try again
      --retry_cnt;
    }
		
    if (hp != NULL) {
      // We succeeded, add the h_addr to sin_addr & set
      // the dns_name_ data member.

      memcpy(&sock_addr_.sin_addr, hp->h_addr, hp->h_length);
      dns_name_ = host;
    } else {
      // Bleh.  Report the error.
      error.Init(EX_UNAVAILABLE, "IPComm::set_peer(): "
                 "gethostbyname(%s) failed: %s", host, hstrerror(h_errno));
      /*
      // TODO(aka) I can't remember if Linux has hstrerror() or not ...
      error.Init("IPComm::set_peer(): gethostbyname(%s) failed: h_errno %d",
      host, h_errno);
      */
    }
  }
#endif
}

// Routine to initialize our IPComm object as a server.  This entails
// installing either INADDR_ANY or in6addr_any in as our Internet
// address depending on which address family is passed in to the
// routine.
//
// Note, this routine can set an ErrorHandler event.
void IPComm::InitServer(const int address_family) {
  set_address_family(address_family); 
    
  switch (address_family_) {
    case AF_INET :
      sockaddr_.in_.sin_addr.s_addr = INADDR_ANY;
      break;

    case AF_INET6 :
      sockaddr_.in6_.sin6_addr = in6addr_any;
      break;

    default :
      error.Init(EX_SOFTWARE, "IPComm::InitServer(): "
                 "unknown address family: %d", address_family_);
      return;
  }
}

// Routine to open a file descriptor for network I/O.
//
// Note, this routine can set an ErrorHandler event.
void IPComm::Socket(const int domain, const int type, const int protocol) {
  if ((descriptor_->fd_ = socket(domain, type, protocol)) < 0) {
    error.Init(EX_IOERR, "IPComm::Socket(): socket(%d, %d, %d): %s", 
               domain, type, protocol, strerror(errno));
    return;
  }

  // Set file descriptor for (non)blocking operations.
  if (IsBlocking()) {
    set_socket_blocking();
  } else {
    set_socket_nonblocking();
  }

  // Set file descriptor for OPEN|CLOSE_ON_EXEC behavior.
  if (IsOpenOnExec()) {
    set_socket_open_on_exec();
  } else {
    set_socket_close_on_exec();
  }
}

// This routine gets the status of a specific option on a socket.
//
// Note, this routine can set an ErrorHandler event.
int IPComm::Getsockopt(int level, int option_name,
                       void* option_value, socklen_t* option_len) const {
  if (address_family_ == AF_UNSPEC) {
    error.Init(EX_SOFTWARE, "IPComm::Getsockopt(): "
               "address_family is AF_UNSPEC");
    return -1;
  }

  if (descriptor_->fd_ == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "IPComm::Getsockopt(): descriptor is NULL");
    return -1;
  }

  // Note, Solaris needs char* for option_value.

  // Normally, this routine will be called with "level" set to
  // SOL_SOCKET, and "option_name" set to SO_ERROR, prior to
  // reading|writing on the socket (in-order to check the socket's
  // ERROR status). *option_value will be set (in this case) to one of
  // the following:
  //
  // 	EALREADY	// still not ready, just return
  //  	ETIMEDOUT	// "Connection timed out"
  //	ECONNRESET	// "Connection reset by peer"
  //	ECONNREFUSED 	// "Connection refused"
  //
  // or 0, if there is no ERROR on the socket, i.e., it's okay to
  // read|write.

  // Get the socket status.
  if (getsockopt(descriptor_->fd_, level, option_name,  
                 option_value, option_len)) {
    error.Init(EX_IOERR, "IPComm::Getsockopt(): "
               "getsockopt(%d, %d, %d, void*, %d) failed: %s",
               descriptor_->fd_, level, option_name, *option_len,
               strerror(errno));
    return -1;
  }

  // HACK: As most socket-level options use an int, let's for
  // convenience, convert the option value (a void*) to an int and
  // return it.

  // TOOD(aka) Hmm, this cast isn't working ... not surprising.
  uint64_t tmp_val = reinterpret_cast<uint64_t>(option_value);
  int socket_opt_val = static_cast<int>(tmp_val);
  return socket_opt_val;  // for convenience
}

// This routine sets the options on a socket.
//
// Note, this routine can set an ErrorHandler event.
void IPComm::Setsockopt(int level, int option_name,
                        const void* option_value, socklen_t optlen) {
  if (address_family_ == AF_UNSPEC) {
    error.Init(EX_SOFTWARE, "IPComm::Setsockopt(): "
               "address_family is AF_UNSPEC");
    return;
  }

  if (descriptor_->fd_ == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "IPComm::Setsockopt(): descriptor is NULL");
    return;
  }

  socklen_t len = sizeof(int);

  // Set the socket options.
  if (setsockopt(descriptor_->fd_, level, option_name, option_value, optlen)) {
    error.Init(EX_IOERR, "IPComm::Setsockopt(): "
               "setsockopt(%d, %d, %d, option_value, %d) failed: %s",
               descriptor_->fd_, level, option_name, len, strerror(errno));
    return;
  }
}
	
// Routine to slurp up data off the wire using recvfrom(2).
//
// Note, this routine can set an ErrorHandler event.
ssize_t IPComm::Recvfrom(char* buf, const size_t len, 
                         struct sockaddr* from, socklen_t* from_len) {
  ssize_t n = 0;
  if ((n = recvfrom(descriptor_->fd_, buf, len, 0, from, from_len)) < 0) {
    if (! IsBlocking() && (errno == EAGAIN)) {
      return n;	// no more work, return
    } else {
      // Other than EAGAIN means problems!
      error.Init(EX_IOERR, "IPComm::Recvfrom(): rcvfrom(fd: %d) failed: %s",
                 descriptor_->fd_, strerror(errno));
      return 0;
    }
  }

  _LOGGER(LOG_DEBUG, "IPComm::Recvfrom(): "
          "Received %d byte(s) from %s:%hu.", 
          n, inet_ntoa(((sockaddr_in*)from)->sin_addr),
          ntohs(((sockaddr_in*)from)->sin_port));

  return n;
}

// Routine to send data out onto the wire via sendto(2).
//
// Note, this routine can set an ErrorHandler event.
ssize_t IPComm::Sendto(const char* buf, const size_t len, 
                       const struct sockaddr* new_peer, 
                       const socklen_t peer_len) {

  // Determine if we want to use a different peer than what's in our
  // object, i.e., we could be the server in a client/server model.

  const struct sockaddr* to;
  socklen_t to_len;
  if (new_peer != NULL) {
    to = new_peer;
    to_len = peer_len;
  } else {
    if (address_family_ == AF_UNSPEC) {
      error.Init(EX_SOFTWARE, "IPComm::Sendto(): address_family is AF_UNSPEC");
      return 0;
    }

    switch (address_family_) {
      case AF_INET : 
        to = (const struct sockaddr*)&sockaddr_.in_;
        to_len = sizeof(sockaddr_.in_);
        break;

      case AF_INET6 : 
        to = (const struct sockaddr*)&sockaddr_.in6_;
        to_len = sizeof(sockaddr_.in6_);
        break;

      default :
        error.Init(EX_SOFTWARE, "IPComm::Sendto(): "
                   "unknown address_family: %d.", address_family_);
        return 0;
    }
  }

  ssize_t bytes_left = (ssize_t)len;
  ssize_t n = 0;
  off_t offset = 0;
  for (;;) {
    if ((n = sendto(descriptor_->fd_, buf + offset, len, 0, to, to_len)) < 0) {
      if (! IsBlocking() && errno == EAGAIN)	
        break;	// unable to write now
      else {
        // Bleh, a genuine error.
        error.Init(EX_IOERR, "IPComm::Sendto(): write(fd: %d) failed: %s",
                   descriptor_->fd_, strerror(errno));
        return 0;
      }
    }

    bytes_left -= n;
    offset += (off_t)n;

    if (bytes_left <= 0)
      break;  // we're done
  }

  _LOGGER(LOG_DEBUG, "IPComm::Sendto(): Sent %d byte(s) to %s:%hu.", 
          len - bytes_left, inet_ntoa(((sockaddr_in*)to)->sin_addr),
          ntohs(((sockaddr_in*)to)->sin_port));

  return (len - bytes_left);
}
	
// Routine to slurp up data off the wire using recvmsg(2).	
//
// Note, this routine can set an ErrorHandler event.
ssize_t IPComm::Recvmsg(struct msghdr* msg, int flags) {
  // recvmsg(3) will populate msghdr* depending on type.
  ssize_t n = 0;
  if ((n = recvmsg(descriptor_->fd_, msg, flags)) < 0) {
    if (! IsBlocking() && (errno == EAGAIN)) {
      return n;	// no more work, return
    } else {
      // Other than EAGAIN means problems!
      error.Init(EX_IOERR, "IPComm::Recvmsg(): rcvfrom(fd: %d) failed: %s",
                 descriptor_->fd_, strerror(errno));
      return 0;
    }
  }

  _LOGGER(LOG_DEBUG, "IPComm::Recvmsg(): Received %d byte(s) from recvmsg.", n);

  return n;
}

// Routine to send data out onto the wire via sendmsg(2).
//
// Note, this routine can set an ErrorHandler event.
ssize_t IPComm::Sendmsg(struct msghdr* msg, int flags) {
  ssize_t len = 0;
  ssize_t bytes_left = 0;  // TODO(aka) This is broken!
  ssize_t n = 0;
  off_t offset = 0;
  for (;;) {
    if ((n = sendmsg(descriptor_->fd_, msg, flags)) < 0) {
      if (! IsBlocking() && errno == EAGAIN)	
        break;	// unable to write now
      else {
        // Argh, a genuine error.
        error.Init(EX_IOERR, "IPComm::Sendmsg(): write(fd: %d) failed: %s",
                   descriptor_->fd_, strerror(errno));
        return 0;
      }
    }

    bytes_left -= n;
    offset += (off_t)n;

    if (bytes_left <= 0)
      break;	// we're done
  }

  _LOGGER(LOG_DEBUG, "IPComm::Sendmsg(): Sent %d byte(s) via sendmsg.", 
          len - bytes_left);

  return (len - bytes_left);
}

// Routine to close an open socket.
void IPComm::Close(void) {
  if (descriptor_->fd_ > DESCRIPTOR_NULL) {
    if (close(descriptor_->fd_) < 0) {
      switch (errno) {
        case EINTR :
          // Fall-through.
        case EBADF :
          // Fall-through.
        default :
          _LOGGER(LOG_WARNING, "IPComm::Close(): close() failed: %s.", 
                  strerror(errno));
      }
    }

    descriptor_->fd_ = DESCRIPTOR_NULL;

    // TODO(aka) It would be nice here if we knew if we had
    // *connected* first (i.e., TCPConn::connected_).  The only way I
    // can think to make that work is move the data member to IPComm,
    // but that just seems wrong.

    _LOGGER(LOG_DEBUG, "IPComm::Close(): Closed connection with %s.", 
            hostname().c_str());
  }
}

// Routine to *resolve* the Internet address in our sockaddr_ union,
// i.e., (struct sockaddr_in[6])in[6]_.(struct
// in[6]_addr)sin[6]_addr.(in_addr_t or uint8_t[16])s[6]_addr value
// with getnameinfo(3).
//
// Note, this routine can *block* in getnameinfo(3).
//
// This routine can set and ErrorHandler event.
void IPComm::ResolveDNSName(int retry_cnt) {
  if (address_family_ == AF_UNSPEC) {
    error.Init(EX_SOFTWARE, "IPComm::ResolveDNSName(): "
               "address_family is AF_UNSPEC");
    return;
  }

  int ecode = 0;
  string host(NI_MAXHOST + 1, '\0');  // '\0' so strlen() works, +1
                                      // cause getnameinfo(3) says you
                                      // need to additionally account
                                      // for the null terminator?!?

  // Attempt to resolve sockaddr_ union.
  while (1) {
    // We use NI_NAMEREQD, as we are *trying* to resolve the address!
    switch (address_family_) {
      case AF_INET : 
        ecode = getnameinfo((struct sockaddr*)&sockaddr_.in_, 
                            sizeof(sockaddr_.in_),
                            (char*)host.c_str(), NI_MAXHOST, NULL, 0, 
                            NI_NAMEREQD);
        break;

      case AF_INET6 : 
        ecode = getnameinfo((struct sockaddr*)&sockaddr_.in6_,
                            sizeof(sockaddr_.in6_),
                            (char*)host.c_str(), NI_MAXHOST, NULL, 0,
                            NI_NAMEREQD);
        break;

      default :
        error.Init(EX_SOFTWARE, "IPComm::ResolveDNSName(): "
                   "unknown address_family: %d.", address_family_);
        return;
    }

    if (ecode == 0 || --retry_cnt == 0 || ecode != EAI_AGAIN)
      break;  // either it worked, we hit an error or we're out of time
    else {
      _LOGGER(LOG_DEBUG, "IPComm::ResolveDNSName() retry_cnt = %d && ecode == %d.", retry_cnt, ecode);
      sleep(1);
    }
  }

  // See if we still have an error condition.
  if (ecode) {
    if (ecode == EAI_SYSTEM)
      error.Init(EX_OSERR, "IPComm::ResolveDNSName(): %s", 
                 strerror(errno));
    else
      error.Init(EX_OSERR, "IPComm::ResolveDNSName(): %s", 
                 gai_strerror(ecode));
    dns_names_.clear();
    return;
  }

  dns_names_.push_back(host);  // add our resolved name to our cache

#if 0
  // TODO(aka) The gethostbyaddr(3) way, which would give us *all*
  // CNAMES as well.  However, gethostbyaddr(3) apparently is not
  // reentrant.

  struct hostent* hp = NULL;
  while (1) {
    switch (address_family_) {
      case AF_INET : 
        hp = gethostbyaddr(sockaddr_.in_.sin_addr, 
                           sizeof(sockaddr_.in_.sin_addr), AF_INET);
        break;

      case AF_INET6 : 
        hp = gethostbyaddr(sockaddr_.in6_.sin6_addr, 
                           sizeof(sockaddr_.in6_.sin6_addr), AF_INET6);
        break;

      default :
        warnx("IPComm::ResolveDNSName(): unknown address_family: %d.",
              address_family_);
        error++;
    }

    if (hp != NULL || !--retry_cnt || h_errno != TRY_AGAIN)
      break;
    else {
      _LOGGER(LOG_DEBUG, "IPComm::ResolveDNSName() retry_cnt = %d && ecode == %d.", retry_cnt, ecode);
      sleep(1);
    }
  }

  if (hp) {

    // TODO(aka) This block is *not* complete.  First, we need to test the
    // address_family type.  Additionally, we need to figure out how
    // to get the v6 address out of h_addr_list .. I'm guessing we
    // cast it as an in6_addr**, but we'd have to test that.

    struct in_addr** addr_list = (struct in_addr**)hp->h_addr_list;
    for(i = 0; addr_list[i] != NULL; i++) {
      dns_names_.push_back(inet_ntoa(*addr_list[i]));
    }
  }
#endif

#if 0
  // TODO(aka) Deprecated IPv4-only method.
  struct hostent* hp = NULL;
  while (! (hp = gethostbyaddr((char*)&sock_addr_.sin_addr, 
                               sizeof(sock_addr_.sin_addr), 
                               sock_addr_.sin_family)) && 
         h_errno == TRY_AGAIN && retry_cnt) {
    sleep(1);	// sleep & try again
    --retry_cnt;
  }
		
  if (hp) {
    // We succeeded, set the dns_name_ data member.
    dns_name_ = hp->h_name;
  } else {

    // TODO(aka) Argh, DNS failure!  For now, leave dns_name_ alone,
    // because hostname() returns inet_ntoa() if dns_name_ is blank.
    // This will allow us *someday* to come back and re-resolve s_addr
    // if dns_name_ is empty!

    dns_name_.clear();
  }
#endif
}

