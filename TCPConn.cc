// $Id: TCPConn.cc,v 1.5 2013/09/13 14:56:38 akadams Exp $

// Copyright (c) 2008, see the file 'COPYRIGHT.h' for any restrictions.

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>

#include "Logger.h"
#include "TCPConn.h"

#define DEBUG_CLASS 0

#define SCRATCH_BUF_SIZE (1024 * 4)


// Non-class specific utility functions.


// Constructors & destructors functions.
TCPConn::TCPConn(void) {
#if DEBUG_CLASS
  warnx("TCPConn::TCPConn(void) called.");
#endif

  connected_ = false;
  listening_ = false;
}

TCPConn::~TCPConn(void) {
#if DEBUG_CLASS
  warnx("TCPConn::~TCPConn(void) called.");
#endif

  // All the work is done in IPComm.
}

// Copy constructor, assignment and equality operator, needed for STL.
TCPConn::TCPConn(const TCPConn& src) 
    : IPComm(src) {
#if DEBUG_CLASS
  warnx("TCPConn::TCPConn(const TCPConn&) called.");
#endif
  
  connected_ = src.connected_;
  listening_ = src.listening_;
}

TCPConn& TCPConn::operator =(const TCPConn& src) {
#if DEBUG_CLASS
  warnx("TCPConn::operator =(const TCPConn&) called.");
#endif

  IPComm::operator =(src);
  connected_ = src.connected_;
  listening_ = src.listening_;

  return *this;
}

// Overloaded operator: equality functions.
int TCPConn::operator ==(const TCPConn& other) const {
#if DEBUG_CLASS
  warnx("TCPConn::operator ==(const TCPConn&) called.");
#endif

  if (IPComm::operator ==(other) == 0)
    return 0;  // if IPComm::operator ==() is false, we're all done

  // If the port and connected flag are equal, we're equal!
  if (port() == other.port() && connected_ == other.connected_ &&
      listening_ == other.listening_)
    return 1;

  return 0;
}

// Accessors.

// Mutators.
void TCPConn::set_connected(const bool connected) {
  connected_ = connected;
}

void TCPConn::clear(void) {
  IPComm::clear();  // IPComm::clear() does all the work
  connected_ = false;
  listening_ = false;
}


// Network manipulation functions.

// Routine to *pretty* print object.
string TCPConn::print(void) const {
  // Print retuns the host, port and socket.
  string tmp_str(SCRATCH_BUF_SIZE, '\0');

  snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s:%hu:%d:%d", 
           IPComm::print().c_str(), port(), connected_, listening_);

  return tmp_str;
}

// Routine to *pretty* print the TCPConn object as a two-tuple
// (host:port).
string TCPConn::print_2tuple(void) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');

  snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s:%hu", 
           IPComm::hostname().c_str(), port());

  return tmp_str;
}

// Routine to *pretty* print the TCPConn object as a three-tuple
// (half-association).
string TCPConn::print_3tuple(void) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');

  snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s:tcp", 
           TCPConn::print_2tuple().c_str());

  return tmp_str;
}

// Copy method.
void TCPConn::Clone(const TCPConn& src) {
  TCPConn::operator =(src);  // use our assignment operator, since we have one
}

// Routine to initialize our TCPConn.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Init(const char* host, const int address_family, int retry_cnt) {
  IPComm::Init(host, address_family, retry_cnt);
  if (error.Event()) {
    error.AppendMsg("TCPConn::Init(): ");
    return;
  }
}

// Routine to initialize our TCPConn as a server.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::InitServer(const int address_family) {
  IPComm::InitServer(address_family);
  if (error.Event()) {
    error.AppendMsg("TCPConn::InitServer(): ");
    return;
  }
}

// Routine to issue a connect(2) on our socket.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Connect(void) {
  int ecode;
  switch (address_family_) {
    case AF_INET :
      ecode = connect(descriptor_->fd_, (struct sockaddr*)&sockaddr_.in_, 
                      sizeof(sockaddr_.in_));
      break;

    case AF_INET6 :
      ecode = connect(descriptor_->fd_, (struct sockaddr*)&sockaddr_.in6_, 
                      sizeof(sockaddr_.in6_));
      break;

    default :
      error.Init(EX_SOFTWARE, "TCPConn::Connect(): "
                 "unknown address_family: %d.", address_family_);
      return;
  }

  if (ecode < 0) {
    if (IsBlocking() && (errno == ECONNREFUSED || errno == ETIMEDOUT)) {
      error.Init(EX_IOERR, "TCPConn::Connect(void): connect(%s:%hu): %s",
                 hostname().c_str(), port(), strerror(errno));
      return;
    } else if (!IsBlocking() && (errno == EINPROGRESS)) {
      // Hmm, nothing to do.
      _LOGGER(LOG_INFO, "Connecting to: %s:%hu.",
              hostname().c_str(), port());
      return;
    } else {
      if (errno == EINVAL) {
        // Hmm, programming error?
        error.Init(EX_IOERR, "TCPConn::Connect(void): connect(%s:%hu), "
                   "family %d: %s",
                   ip_address().c_str(), port(), address_family(),
                   strerror(errno));

      } else {
        // System ERROR ...
        error.Init(EX_IOERR, "TCPConn::Connect(void): connect(%s:%hu): %s",
                   ip_address().c_str(), port(), strerror(errno));
      }
      return;
    }
  }	

  connected_ = true;
}

// Routine to issue a connect(2) on a socket.
//
// TOOD(aka) This routine needs deprecated.  Users should simply use
// TCPConn::Init() followed by TCPConn::Connect(), which is pretty
// much all that this routine does!
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Connect(const char* host, const in_port_t port, 
                      const int address_family) {
  // Initialize object.
  IPComm::Init(host, address_family, IPCOMM_DNS_RETRY_CNT);
  IPComm::set_port(port);
  if (error.Event()) {
    error.AppendMsg("TCPConn::Connect(%s, %hu, %d)", 
                    host, port, address_family);
    return;
  }

  // Call TCPConn::Connect(void) to get the work done.
  Connect();
}

// Routine to bind(2) a name to an unnmaed socket.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Bind(void) {

  // Note, if port is 0 (IPCOMM_PORT_NULL), then the kernel will
  // assign an ephemeral local port.  The caller will then need to
  // call TCPConn::Getpeername() to get the local-process (port).

  int ecode;
  switch (address_family_) {
    case AF_INET :
      ecode = bind(descriptor_->fd_, (struct sockaddr*)&sockaddr_.in_, 
                   sizeof(sockaddr_.in_));
      break;

    case AF_INET6 :
      ecode = bind(descriptor_->fd_, (struct sockaddr*)&sockaddr_.in6_, 
                   sizeof(sockaddr_.in6_));
      break;

    default :
      error.Init(EX_SOFTWARE, "TCPConn::Bind(): "
                 "unknown address_family: %d.", address_family_);
      return;
  }

  if (ecode < 0) {
    error.Init(EX_IOERR, "TCPConn::Bind(): bind(%hu): %s",
               port(), strerror(errno));
  }
}

// Routine to bind(2) a name to an unnmaed socket, using an ephemeral
// address.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Bind(const in_port_t port) {
  set_port(port);  // set it
		
  TCPConn::Bind();  // call TCPConn::Bind() to get the work done
}

// Routine to issue listen(2) on a socket.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Listen(const int backlog) {
  if (listen(descriptor_->fd_, backlog) < 0) {
    error.Init(EX_IOERR, "TCPConn::Listen(): listen(%hu): %s", 
               port(), strerror(errno));
    return;
  }

  listening_ = true;
  switch (address_family_) {
    case AF_INET :
      _LOGGER(LOG_NOTICE, "IPv4 server listening on port: %hu.", port());
      break;

    case AF_INET6 :
      _LOGGER(LOG_NOTICE, "IPv6 server listening on port: %hu.", port());
      break;

    default :
      error.Init(EX_SOFTWARE, "TCPConn::Listen(): "
                 "unknown address_family: %d.", address_family_);
      return;
  }
}

// Routine to accept(2) a connection on a socket.  The calling routine
// passes in the peer's TCPConn object.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Accept(TCPConn* client) const {
  // Get the new socket and install the new sockaddr into our client
  // TCPConn (client.sockaddr_).

  int peer_fd = DESCRIPTOR_NULL;
  socklen_t len;
  switch (address_family_) {
    case AF_INET :
      len = sizeof(client->sockaddr_.in_);
      peer_fd = accept(descriptor_->fd_,
                       (struct sockaddr*)&(client->sockaddr_.in_), &len);
      client->IPComm::set_address_family(client->sockaddr_.in_.sin_family);
      break;

    case AF_INET6 :

      // TODO(aka) If it's possible that we can *accept* an IPv4
      // connection on our IPv6 listening socket, then we'll have to
      // do something different in here.  Perhaps, we can accept() on
      // a sock_storage struct, and then figure out what we have?

      len = sizeof(client->sockaddr_.in6_);
      peer_fd = accept(descriptor_->fd_,
                       (struct sockaddr*)&(client->sockaddr_.in6_), &len);
      client->IPComm::set_address_family(client->sockaddr_.in6_.sin6_family);
      break;

    default :
      error.Init(EX_SOFTWARE, "TCPConn::Accept(): "
                 "unknown address_family: %d.", address_family_);
      return;
  }

  if (peer_fd < 0) {
    if (!IsBlocking() && (errno == EAGAIN)) {
      // Hmm, someone isn't using select(2) ...
      error.Init(EX_IOERR, "TCPConn::Accept(): accept(): %s, "
                 "Note, Accept() expects the fd to be ready via poll(2)!",
                 strerror(errno));
      return;
    } else {
      error.Init(EX_IOERR, "TCPConn::Accept(): accept(): %s", 
                 strerror(errno));
      return;
    }
  }

  // Sanity check len.
  switch (address_family_) {
    case AF_INET :
      if (len != sizeof(client->sockaddr_.in_)) { 
        // This doesn't warrant setting an ErrorHandler event ...
        _LOGGER(LOG_WARNING, "TCPConn::Accept(): TODO(aka) "
                "peer sockaddr_ is %d bytes after "
                "accept(2), but was expecting it to be %d bytes!", 
                len, (socklen_t)sizeof(client->sockaddr_.in_));
      }
      break;

    case AF_INET6 :
      if (len != sizeof(client->sockaddr_.in6_)) {
        // This doesn't warrant setting an ErrorHandler event ...
        _LOGGER(LOG_WARNING, "TCPConn::Accept(): TODO(aka) "
                "peer sockaddr_ is %d bytes after "
                "accept(2), but was expecting it to be %d bytes!", 
                len, (socklen_t)sizeof(client->sockaddr_.in6_));
      }
      break;

    default :
      error.Init(EX_SOFTWARE, "TCPConn::Accept(): In socklen_t check "
                 "unknown address_family: %d.", address_family_);
      return;
  }

  client->set_fd(peer_fd);
  client->connected_ = true;

  if (!IsBlocking())
    client->set_nonblocking();

  client->IPComm::ResolveDNSName(IPCOMM_DNS_RETRY_CNT);  // resolve peer's name

  _LOGGER(LOG_NOTICE, "Connection from: %s.", 
          client->print_3tuple().c_str());
}

// Routine to accept(2) a connection on a socket. This routined
// provides the peer's TCPConn object.
//
// Note, this routine can set an ErrorHandler event.
TCPConn TCPConn::Accept(void) const {
  TCPConn client;  // XXX TODO(aka) We need framing type here!  Why?
                   // Framing is part of TCPSession, not TCPConn?

  // Call TCPConn::Accept(TCPConn*) to get the work done.
  Accept(&client);

  return client;
}

// Routine to issue close(2) on a socket.
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Close(void) {
  IPComm::Close();  // use IPComm to get the work done

  if (IsListening())
    _LOGGER(LOG_DEBUG, "TCPConn::Close(): Closed TCP listen socket.");
  else
    _LOGGER(LOG_NOTICE, "Closed connection with %s.",
            print_3tuple().c_str());

  connected_ = false;
  listening_ = false;
}

// Routine to retrieve the local-address and local-process (port) from
// a connected socket via getsockname(2).
//
// Note, this routine can set an ErrorHandler event.
void TCPConn::Getsockname(struct sockaddr* address, 
                          socklen_t* address_len) const {
  if (address_family_ == AF_UNSPEC) {
    error.Init(EX_SOFTWARE, "TCPConn::Getsockname(): "
               "address_family is AF_UNSPEC");
    return;
  }

  if (!IsConnected()) {
    error.Init(EX_SOFTWARE, "TCPConn::Getsockname(): "
               "socket not connected");
    return;
  }

  int ecode = getsockname(descriptor_->fd_, address, address_len);
  if (ecode < 0) {
    error.Init(EX_IOERR, "TCPConn::Getsockname(): getsockname(): %s",
               hostname().c_str(), port(), strerror(errno));
    return;
  }
}

// Routine to issue write(2) on a open socket until everything has
// been written or the socket has become unavailable.
//
// Note, this routine can set an ErrorHandler event.
ssize_t TCPConn::Write(const char* buf, const ssize_t buf_len) const {
  ssize_t bytes_wrote = 0;
  ssize_t bytes_left = buf_len;
  ssize_t n = 0;
  off_t offset = 0;
  for (;;) {
    if ((n = write(descriptor_->fd_, buf + offset, buf_len - offset)) < 0) {
      if (! IsBlocking() && errno == EAGAIN)	
        break;  // this is okay, the socket is simply not ready yet
      else {
        // Bleh, a genuine error.
        error.Init(EX_IOERR, "TCPConn::Write(): write(fd: %d) failed: %s",
                   descriptor_->fd_, strerror(errno));
        return 0;
      }
    }

    bytes_left -= n;
    offset += (off_t)n;

    if (bytes_left <= 0)
      break;  // we're done, yeah
  }

  bytes_wrote = buf_len - bytes_left;

  _LOGGER(LOG_DEBUG, "TCPConn::Write(): Wrote %d byte(s) to: %s.", 
          bytes_wrote, print().c_str());

  return bytes_wrote;
}	

// This routine calls read(2) on an ready socket.	
//
// Note, this routine can set an ErrorHandler event.
//  
// TODO(aka) It might be cool if we overload Read() to include a
// version that uses a STL::string (or STL::data?) parameter instead
// of char*, in-order to be able to resize the buffer while in this
// routine ...
ssize_t TCPConn::Read(const ssize_t buf_len, char* buf, bool* eof) const {
  *eof = false;

  ssize_t n = read(descriptor_->fd_, buf, buf_len);
  if (n == 0) {
    *eof = true;  // we got EOF
  } else if (n < 0) {
    // We got an ERROR.
    if (!IsBlocking() && (errno == EAGAIN)) {
      _LOGGER(LOG_DEBUG, "TCPConn::Read(): EAGAIN on %s.", hostname().c_str());
      return 0;  // socket no longer ready, return
    } else {
      // Argh, anything other than EAGAIN means problems! 
      error.Init(EX_IOERR, "TCPConn::Read(%d): failed: %s", 
                 descriptor_->fd_, strerror(errno));
      return n;
    }
  }

  if (*eof)
    _LOGGER(LOG_DEBUG, "TCPConn::Read(): Read EOF from: %s.", print().c_str());

  if (n) {
    _LOGGER(LOG_DEBUG, "TCPConn::Read(): Read %d byte(s) from: %s.", 
            n, print().c_str());
  }

  return n;
}

// This routine calls read(2) on an ready socket, until it either
// returns with an ERROR or EOF.
//
// Note, this routine can set an ErrorHandler event.
ssize_t TCPConn::ReadExhaustive(const ssize_t buf_len, char* buf, 
                                bool* eof) const {
  
  // Note, this routine doesn't make a lot of sense for blocking
  // connections, as read() will block after the first call() waiting
  // for more data (not too mention that the logic in the loop
  // requires seeing an EOF -- which requires the remote side to
  // close() -- to exit cleanly).  Until I've reworked this routine,
  // the current plan is that blocking connections will use
  // ReadLine().

  // TODO(aka) Obviously, this routine needs to be able to handle blocking reads ...

  // TODO(aka) It might be cool is we overload Read() to include a
  // version that uses a STL::string (or STL::data?) parameter instead
  // of char*, in-order to be able to resize the buffer while in this
  // routine ...

  if (IsBlocking()) {
    error.Init(EX_SOFTWARE, "TCPConn::Read(): routine called on blocking socket.");
    return 0;
  }

  *eof = false;

  ssize_t bytes_read = 0;
  ssize_t n = 0;
  ssize_t bytes_left = buf_len;
  off_t offset = 0;
  for (;;) {
    // TODO(aka) This should call TCPConn::Read() to get the work done!
    if ((n = read(descriptor_->fd_, buf + offset, bytes_left)) > 0) {
      offset += n;
      bytes_left -= n;
    } else if (n == 0) {
      *eof = true;  // we got EOF
      break;
    } else {
      if (! IsBlocking() && (errno == EAGAIN)) {
        break;	// socket no longer ready, return
      } else {
        // Argh, anything other than EAGAIN means problems! 
        error.Init(EX_IOERR, "TCPConn::Read(): failed: %s", 
                   descriptor_->fd_, strerror(errno));
        return offset;
      }
    }
  }

  bytes_read = buf_len - bytes_left;

  _LOGGER(LOG_INFO, "Read %d byte(s) from: %s.",
          bytes_read, hostname().c_str());

  return bytes_read;
}

// This routine calls read(2) on a ready socket, however, it reads
// until a framing (or '\0') characters is read.
//
// Note, this routine can set an ErrorHandler event.
ssize_t TCPConn::ReadLine(const char delimiter, const ssize_t buf_len, 
                          char* buf, bool* eof) const {
	
  // Note, this routine only reads one character at a time, so it is
  // horribly inefficient, but we do need *something* for blocking I/O
  // ...

  // TODO(aka) I'm not sure if I actually need an EOF indicator, i.e.,
  // is it possible to just read a carriage return?  (In which case I
  // would return 0 but not EOF ...)

  // TODO(aka) I need to change this routine to read() into a static
  // cache, and then parse that buffer for our delimiter(s).

  *eof = false;

  ssize_t bytes_read = 0;
  ssize_t n = 0;
  char* ptr = buf;
  char c;
  for (bytes_read = 0; bytes_read < (buf_len - 1); bytes_read++) {
    if ((n = read(descriptor_->fd_, &c, 1)) == 1) {
      if (! c || c == '\0') {
        _LOGGER(LOG_DEBUG, "TCPConn::ReadLine(): null at byte %d.", bytes_read);
        break;  // don't increment ptr or counter
      } else if (c == '\r') {
        _LOGGER(LOG_DEBUG, "TCPConn::ReadLine(): \\r at byte %d.", bytes_read);

        // TODO(aka) Why do I need this?  If we want to break of 
        break;  // don't increment ptr or counter
      } else if (c == delimiter) {
        _LOGGER(LOG_DEBUG, "TCPConn::ReadLine(): delimiter at byte %d.", bytes_read);

        // We're all done for now, head back.
        break;  // don't increment ptr or counter
      }

      *ptr++ = c;
    } else if (n == 0) {
      *eof = true;
      if (bytes_read == 0)
        return 0;  // EOF, but no data read
      else
        break;	// EOF, *and* data was read
    } else {
      error.Init(EX_IOERR, "TCPConn::ReadLine(): read(fd: %d): %s",
                 descriptor_->fd_, strerror(errno));
      return 0;
    }
  }

  *ptr = '\0';

  if (bytes_read) {
    _LOGGER(LOG_INFO, "Read %d byte(s) from: %s.",
            bytes_read, hostname().c_str());

    _LOGGER(LOG_DEBUG, "TCPConn::ReadLine(): read: %s.", buf);
  }

  return bytes_read;
}

// Boolean functions.

bool TCPConn::Equals(const TCPConn& other) const {
  // Use TCPConn::operator ==() to get the job done.
  if (TCPConn::operator ==(other) == 0)
    return false;

  return true;
}

// Debugging stuff functions.


// Obsoleted.
#if 0
// TODO(aka) I'm 99% sure that we do not need these ...
TCPConn::TCPConn(const in_port_t port, const in_addr_t host_in_addr,
                 const int family)
    : IPComm(host_in_addr, family) {
  sock_addr_.sin_port = htons(port);
  connected_ = true;
  listen;
}

TCPConn::TCPConn(const in_port_t port, const char* host, const int family)
    : IPComm(host, family) {
  sock_addr_.sin_port = htons(port);
  connected_ = true;
  listen;
}
#endif

