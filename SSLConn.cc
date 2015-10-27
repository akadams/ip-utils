// $Id: SSLConn.cc,v 1.5 2013/09/13 14:56:38 akadams Exp $

// Copyright (c) 2008, see the file 'COPYRIGHT.h' for any restrictions.

#include <sys/stat.h>

#include <openssl/err.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "Logger.h"
#include "SSLConn.h"


#define SSL_X509_MAX_FIELD_SIZE 200
#define SCRATCH_BUF_SIZE (1024 * 4)

#define DEBUG_CLASS 0
#define DEBUG_INCOMING_DATA 0

// Non-class specific utility functions.


// Constructors & destructors functions.
SSLConn::SSLConn(void) {
#if DEBUG_CLASS
  warnx("SSLConn::SSLConn(void) called.");
#endif

  ssl_ = NULL;
  peer_certificate_ = NULL;
}

SSLConn::~SSLConn(void) {
#if DEBUG_CLASS
  warnx("SSLConn::~SSLConn(void) called.");
#endif

  if (peer_certificate_ != NULL) {
    X509_free(peer_certificate_);
    peer_certificate_ = NULL;
  }

  // If IPComm's destructor (via TCPConn) is about to close the socket, free our SSL*.
  if (ssl_ != NULL && descriptor_->cnt_ == 1) {
    SSL_free(ssl_); 
    ssl_ = NULL;
  }

  // Rest of work is done in IPComm (via TCPConn).
}

// Copy constructor, assignment and equality operator, needed for STL.
SSLConn::SSLConn(const SSLConn& src) 
    : TCPConn(src) {
#if DEBUG_CLASS
  warnx("SSLConn::SSLConn(const SSLConn&) called.");
#endif

  ssl_ = src.ssl_;  // note, ref count in Descriptor is bumped in IPComm copy constructor

  peer_certificate_ = SSL_get_peer_certificate(ssl_);  // SSL_get_peer_cerfificate() ref counts
}

SSLConn& SSLConn::operator =(const SSLConn& src) {
#if DEBUG_CLASS
  warnx("SSLConn::operator =(const SSLConn&) called.");
#endif

  // If we're about to blow away our IPComm -> Descriptor, then we
  // need to blow away our SSL* object before setting it to the new one.

  if (ssl_ != NULL && descriptor_->cnt_ == 1)
    SSL_free(ssl_);
  ssl_ = src.ssl_;

  // peer_certificate may or may not have already been alocated ...
  if (peer_certificate_ != NULL)
    X509_free(peer_certificate_);
  peer_certificate_ = SSL_get_peer_certificate(ssl_);  // SSL_get_peer_cerfificate() ref counts

  TCPConn::operator =(src);  // finally, get the majority of the work done in TCPConn

  return *this;
}

// Overloaded operator: equality functions.
int SSLConn::operator ==(const SSLConn& other) const {
#if DEBUG_CLASS
  warnx("SSLConn::operator ==(const SSLConn&) called.");
#endif

  return TCPConn::operator ==(other);
}

// Accessors.

// Mutators.
void SSLConn::clear(void) {
  // If we're about to blow away our IPComm -> Descriptor, then we
  // need to blow away our SSL* object before setting it to the new one.

  if (ssl_ != NULL && descriptor_->cnt_ == 1)
    SSL_free(ssl_);
  ssl_ = NULL;

  // peer_certificate may or may not have already been alocated ...
  if (peer_certificate_ != NULL) {
    X509_free(peer_certificate_);
    peer_certificate_ = NULL;
  }

  TCPConn::clear();  // get the rest of the work done
}


// Network manipulation functions.

// Routine to *pretty* print object.
string SSLConn::print(void) const {
  // Print retuns the host, port and socket.
  string tmp_str(SCRATCH_BUF_SIZE, '\0');

  snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s", 
           TCPConn::print().c_str());

  return tmp_str;
}

// Routine to initialize our SSLConn.
//
// Note, this routine can set an ErrorHandler event.
void SSLConn::Init(const char* host, const int address_family, int retry_cnt) {
  IPComm::Init(host, address_family, retry_cnt);
  if (error.Event()) {
    error.AppendMsg("SSLConn::Init(): ");
    return;
  }
}

// Routine to initialize our SSLConn as a server.
//
// Note, this routine can set an ErrorHandler event.
void SSLConn::InitServer(const int address_family) {
  IPComm::InitServer(address_family);
  if (error.Event()) {
    error.AppendMsg("SSLConn::InitServer(): ");
    return;
  }
}

// Routine to open a socket.
//
// Note, this routine can set an ErrorHandler event.
void SSLConn::Socket(const int domain, const int type, const int protocol,
                     SSLContext* ctx) {
  IPComm::Socket(domain, type, protocol);
  if (error.Event()) {
    error.AppendMsg("SSLConn::Socket(): ");
    return;
  }

  // Okay, we now have a file desciptor, so get a SSL* object (which
  // we reference count via the Descriptor ojbect).

  if ((ssl_ = SSL_new(ctx->ctx_)) == NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Socket(): SSL_new(3) failed: %s", 
               ssl_err_str().c_str());
    return;
  }
}

// Routine to issue a connect(2) on our socket.
//
// Note, this routine can set an ErrorHandler event.
void SSLConn::Connect(void) {
  if (ssl_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Connect(): SSL* object NULL!");
    return;
  }

  TCPConn::Connect();  // get the majority of the work done
  if (error.Event()) {
    error.AppendMsg("SSLConn::Connect(): ");
    return;
  }

  // Associate the TCP file descriptor to our SSL* object.
  if (!SSL_set_fd(ssl_, fd())) {
    error.Init(EX_SOFTWARE, "SSLConn::Connect(): SSL_set_fd(3) failed: %s", 
               ssl_err_str().c_str());
    return;
  }

  // NONBLOCKING: We may need to account for EALREADY in here, or at
  // least see what's going on in TCPConnect() (and possibly in
  // main.cc).

  int ret = SSL_connect(ssl_);
  if (ret == 0) {
    // Check the SSL ERROR condition ...
    switch(SSL_get_error(ssl_, ret)) {
      case SSL_ERROR_ZERO_RETURN :  // the connection is closed?
        {
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): %s terminated connection",
                     hostname().c_str());
          return;
        }
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          // EOF, remote end closed abruptly ...
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): "
                     "Recevied EOF while trying to SSL_connect() to %s on %d",
                     hostname().c_str(), fd());
          return;
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): SSL_ERROR_SYSCALL: "
                     "SSL_connect() to %s failed: %s",
                     hostname().c_str(), ssl_err_str().c_str());
          return;
        }
        break;

      case SSL_ERROR_SSL :
        {
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): Shutdown: SSL_ERROR_SSL: %s",
                     ssl_err_str().c_str());
          return;
        }
        break;

      default:
        error.Init(EX_SOFTWARE, "SSLConn::Connect(): unknown ERROR: %s", ssl_err_str().c_str());
        return;
    }  // switch(SSL_get_error(ssl_, ret)) {
  } else if (ret < 0) {
    // Check the SSL ERROR condition ...
    switch(SSL_get_error(ssl_, ret)) {
      case SSL_ERROR_WANT_READ :
        // Fall-through.

      case SSL_ERROR_WANT_WRITE :
        if (IsBlocking()) {
          // WTF!?!
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): SSL_ERROR_WANT_READ/SSL_ERROR_WANT_WRITE: "
                     "on blocking connection to %s (fd %d)", hostname().c_str(), fd());
          return;
        } else {
          _LOGGER(LOG_INFO, "SSLConn::Connect(): received SSL_ERROR_WANT_READ/WRITE, returning.");
          return;
        }
        break;

#if defined SSL_ERROR_WANT_ACCEPT  // 0.9.6g does not have WANT_ACCEPT
      case SSL_ERROR_WANT_ACCEPT :
        // Fall-through.
#endif

      case SSL_ERROR_WANT_CONNECT :
        if (IsBlocking()) {
          // WTF!?!
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): "
                     "SSL_ERROR_WANT_ACCEPT/SSL_ERROR_WANT_CONNECT :"
                     "on blocking connection to %s (fd %d)", hostname().c_str(), fd());
          return;
        } else {
          _LOGGER(LOG_INFO, "SSLConn::Connect(): "
                  "Received SSL_ERROR_WANT_ACCEPT/CONNECT: returning.");
          return;
        }
        break;

      case SSL_ERROR_WANT_X509_LOOKUP :  // something's up with SSL_CTX_set_client_cert_cb()
        {
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): "
                     "SSL_ERROR_WANT_X509_LOOKUP: with host %s (fd %d)",
                     hostname().c_str(), fd());
          return;
        }
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          // I/O error, check errno.
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): SSL_ERROR_SYSCALL: "
                     "I/O error with %s on fd %d: %s",
                     hostname().c_str(), fd(), strerror(errno));
          return;
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): SSL_ERROR_SYSCALL: %s", 
                     ssl_err_str().c_str());
          return;
        }
        break;

      case SSL_ERROR_SSL :
        {
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): SSL_ERROR_SSL: %s", ssl_err_str().c_str());
          return;
        }
        break;

      default:
        {
          error.Init(EX_SOFTWARE, "SSLConn::Connect(): unknown ERROR: %s", ssl_err_str().c_str());
          return;
        }
    }  // switch(SSL_get_error(ssl_, ret)) {
  }  // else if (ret < 0) {

  peer_certificate_ = SSL_get_peer_certificate(ssl_);  // if peer has a cert, get it

  if (peer_certificate_ != NULL) {
    char cn[SCRATCH_BUF_SIZE];
    X509_NAME* subject = X509_get_subject_name(peer_certificate_);
    X509_NAME_get_text_by_NID(subject, NID_commonName, cn,
                              SSL_X509_MAX_FIELD_SIZE - 1);
    _LOGGER(LOG_NOTICE, "SSL (%s) connection to: %s, received cert: %s.", 
            SSL_CIPHER_get_name(SSL_get_current_cipher(ssl_)),
            hostname().c_str(), cn);
  } else  {
    _LOGGER(LOG_NOTICE, "SSL (%s) connection to: %s.",
            SSL_CIPHER_get_name(SSL_get_current_cipher(ssl_)),
            hostname().c_str());
  }
}

// Routine to accept(2) a connection on a socket.  The calling routine
// passes in the peer's SSLConn object.
//
// Note, this routine can set an ErrorHandler event.
void SSLConn::Accept(SSLConn* peer, SSLContext* ctx) const {
  if (ssl_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Accept(): SSL* object NULL!");
    return;
  }

  TCPConn::Accept(peer);  // get the majority of the work done
  if (error.Event()) {
    error.AppendMsg("SSLConn::Accept(): ");
    return;
  }

  // NONBLOCKING: To avoid the dreaded SSL 'deadlock', let's set a
  // timeout on the TCP socket.

  static struct timeval timeout;
  timeout.tv_sec = 300;	// 5 minutes
  timeout.tv_usec = 0;
  peer->Setsockopt(SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

  // Okay, we now have a file desciptor, so get a SSL* object (which
  // we reference count via the Descriptor ojbect), then associate our
  // TCP file descriptor to it.

  if ((peer->ssl_ = SSL_new(ctx->ctx_)) == NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Accept(): SSL_new(3) failed: %s",
               ssl_err_str().c_str());
    return;
  }
  if (!SSL_set_fd(peer->ssl_, peer->fd())) {
    error.Init(EX_SOFTWARE, "SSLConn::Accept(): SSL_set_fd(3) failed: %s",
               ssl_err_str().c_str());
    return;
  }

  // NONBLOCKING: We may need to account for EALREADY in here, or at
  // least see what's going on in TCPConnect() (and possibly in
  // main.cc).

  int ret = SSL_accept(peer->ssl_);
  if (ret == 0) {
    // Check the SSL ERROR condition ...
    switch(SSL_get_error(ssl_, ret)) {
      case SSL_ERROR_ZERO_RETURN :  // the connection is closed?
        {
          error.Init(EX_SOFTWARE, "SSLConn::Accept: %s terminated connection",
                     peer->hostname().c_str());
          return;
        }
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          // EOF, remote end closed abruptly ...
          error.Init(EX_SOFTWARE, "SSLConn::Accept: "
                     "Recevied EOF while trying to SSL_connect() to %s on %d",
                     peer->hostname().c_str(), fd());
          return;
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Accept: SSL_ERROR_SYSCALL: "
                     "SSL_connect() to %s failed: %s",
                     peer->hostname().c_str(), ssl_err_str().c_str());
          return;
        }
        break;

      case SSL_ERROR_SSL :
        {
          error.Init(EX_SOFTWARE, "SSLConn::Accept: Shutdown: SSL_ERROR_SSL: %s",
                     ssl_err_str().c_str());
          return;
        }
        break;

      default:
        error.Init(EX_SOFTWARE, "SSLConn::Accept: unknown ERROR: %s", ssl_err_str().c_str());
        return;
    }  // switch(SSL_get_error(ssl_, ret)) {
  } else if (ret < 0) {
    // Check the SSL ERROR condition ...
    switch(SSL_get_error(ssl_, ret)) {
      case SSL_ERROR_WANT_READ :
        // Fall-through.

      case SSL_ERROR_WANT_WRITE :
        if (IsBlocking()) {
          // WTF!?!
          error.Init(EX_SOFTWARE, "SSLConn::Accept: SSL_ERROR_WANT_READ/SSL_ERROR_WANT_WRITE: "
                     "on blocking connection to %s (fd %d)", peer->hostname().c_str(), fd());
          return;
        } else {
          _LOGGER(LOG_INFO, "SSLConn::Accept: received SSL_ERROR_WANT_READ/WRITE, returning.");
          return;
        }
        break;

#if defined SSL_ERROR_WANT_ACCEPT  // 0.9.6g does not have WANT_ACCEPT
      case SSL_ERROR_WANT_ACCEPT :
        // Fall-through.
#endif

      case SSL_ERROR_WANT_CONNECT :
        if (IsBlocking()) {
          // WTF!?!
          error.Init(EX_SOFTWARE, "SSLConn::Accept: "
                     "SSL_ERROR_WANT_ACCEPT/SSL_ERROR_WANT_CONNECT :"
                     "on blocking connection to %s (fd %d)",
                     peer->hostname().c_str(), fd());
          return;
        } else {
          _LOGGER(LOG_INFO, "SSLConn::Accept: "
                  "Received SSL_ERROR_WANT_ACCEPT/CONNECT: returning.");
          return;
        }
        break;

      case SSL_ERROR_WANT_X509_LOOKUP :  // something's up with SSL_CTX_set_client_cert_cb()
        {
          error.Init(EX_SOFTWARE, "SSLConn::Accept: SSL_ERROR_WANT_X509_LOOKUP: "
                     "with host %s (fd %d)", peer->hostname().c_str(), fd());
          return;
        }
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          // I/O error, check errno.
          error.Init(EX_SOFTWARE, "SSLConn::Accept: SSL_ERROR_SYSCALL: "
                     "I/O error with %s on fd %d: %s",
                     peer->hostname().c_str(), fd(), strerror(errno));
          return;
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Accept: SSL_ERROR_SYSCALL: %s", 
                     ssl_err_str().c_str());
          return;
        }
        break;

      case SSL_ERROR_SSL :
        {
          error.Init(EX_SOFTWARE, "SSLConn::Accept: SSL_ERROR_SSL: %s", ssl_err_str().c_str());
          return;
        }
        break;

      default:
        {
          error.Init(EX_SOFTWARE, "SSLConn::Accept: unknown ERROR: %s", ssl_err_str().c_str());
          return;
        }
    }  // switch(SSL_get_error(ssl_, ret)) {
  }  // else if (ret < 0) {

  peer->peer_certificate_ = SSL_get_peer_certificate(ssl_);  // if peer has a cert, get it

  if (peer->peer_certificate_ != NULL) {
    char cn[SCRATCH_BUF_SIZE];
    X509_NAME* subject = X509_get_subject_name(peer->peer_certificate_);
    X509_NAME_get_text_by_NID(subject, NID_commonName, cn, SSL_X509_MAX_FIELD_SIZE - 1);
    _LOGGER(LOG_NOTICE, "SSL (%s) connection from: %s, received cert: %s.", 
            SSL_CIPHER_get_name(SSL_get_current_cipher(peer->ssl_)), 
            peer->hostname().c_str(), cn);
  } else {
    _LOGGER(LOG_NOTICE, "SSL (%s) connection from: %s.",
            SSL_CIPHER_get_name(SSL_get_current_cipher(peer->ssl_)), 
            peer->hostname().c_str());
  }
}

// Routine to accept(2) a connection on a socket. This routined
// provides the peer's SSLConn object.
//
// Note, this routine can set an ErrorHandler event.
SSLConn SSLConn::Accept(SSLContext* ctx) const {
  SSLConn peer;  // XXX TODO(aka) We need framing type here!

  // Call SSLConn::Accept(SSLConn*) to get the work done.
  Accept(&peer, ctx);

  return peer;
}

// This routine is either called to *initiate* a shutdown, or to
// respond to a *received* 'close notify'.
//
// Although the TLS standard says that we can send our 'close notify',
// and then close(2) the connection without waiting for our peer's 
// 'close notify', *if* we ever want to re-use connections, we should
// try really hard to wait for the peer's 'close notify' to keep the
// peer's synchornized.
//
// Since it is unclear what SSL_read() is doing when SSL_SHUTDOWN_SENT
// has been set, we are adding a 'uni_directional' flag, that when
// unset, makes us wait around for the peer's 'close notify'.  If it 
// is set, then we simply call TCPConn::Close() after our first 
// SSL_shutdown().
// 
// We *assume* that the calling routine has *already* checked for
// an *incomplete* shutdown (e.g., a TCP FIN, which should have
// set an ERROR condition on the read())
//
// Note, this routine can set an ErrorHandler event.
void SSLConn::Shutdown(const int unidirectional)
{
  if (ssl_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Shutdown(): ssl is NULL");
    return;
  }

  if (fd() == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Shutdown(): %s\'s socket is not open (i.e., fd is %d)",
               hostname().c_str(), DESCRIPTOR_NULL);
    return;
  }

  // From SSL_shutdown(3): When the application is the first party to
  // send the "close notify" alert, SSL_shutdown() will only send the
  // alert and then set the SSL_SENT_SHUTDOWN flag (so that the
  // session is considered good and will be kept in
  // cache). SSL_shutdown() will then return with 0. If a
  // unidirectional shutdown is enough (the underlying connection
  // shall be closed anyway), this first call to SSL_shutdown() is
  // sufficient. In order to complete the bidirectional shutdown
  // handshake, SSL_shutdown() must be called again. The second call
  // will make SSL_shutdown() wait for the peer's "close notify"
  // shutdown alert. On success, the second call to SSL_shutdown()
  // will return with 1. If the peer already sent the "close notify"
  // alert and it was already processed implicitly inside another
  // function (SSL_read(3)), the SSL_RECEIVED_SHUTDOWN flag is
  // set. SSL_shutdown() will send the "close notify" alert, set the
  // SSL_SENT_SHUTDOWN flag and will immediately return with
  // 1. Whether SSL_RECEIVED_SHUTDOWN is already set can be checked
  // using the SSL_get_shutdown() (see also SSL_set_shutdown(3) call.
  // It is therefore recommended, to check the return value of
  // SSL_shutdown() and call SSL_shutdown() again, if the
  // bidirectional shutdown is not yet complete (return value of the
  // first call is 0). As the shutdown is not specially handled in the
  // SSLv2 protocol, SSL_shutdown() will succeed on the first call.

  // Send our 'close notify' and check the return code ...
  int ret = SSL_shutdown(ssl_);
  if (!ret && !unidirectional)
    ret = SSL_shutdown(ssl_);  // wait for their 'close notify'

  // TODO(aka): If we want ever want a "to" or "from", we need to
  // set the TCPConn::server flag!

  if (ret >= 0)
    _LOGGER(LOG_INFO, "Closed SSL connection with: %s.", hostname().c_str());

  // Check for ERRORS ...
  if (ret < 0) {
    switch (SSL_get_error(ssl_, ret)) {
      case SSL_ERROR_WANT_READ :
        // Fall through ...
      case SSL_ERROR_WANT_WRITE :
        {
          if (IsBlocking()) {
            error.Init(EX_SOFTWARE, "SSLConn::Shutdown(): "
                       "Recevived SSL_ERROR_WANT_READ|WRITE on blocking connection with %s",
                       hostname().c_str());
          } else {
            _LOGGER(LOG_DEBUGGING, "SSLConn::Shutdown(): "
                    "Recevived SSL_ERROR_WANT_READ|WRITE on non-blocking connection with %s.",
                    hostname().c_str());
            return;  // go back and wait
          }
        }
        break;

      default :
        error.Init(EX_SOFTWARE, "SSLConn::Shutdown(): Unknown ERROR with %s: %s", 
                   hostname().c_str(), ssl_err_str().c_str());
        break;
    }  // switch (SSL_get_error(ssl_, ret)) {

    // If we made it here, we should have set an ErrorHandler event.
  }

  TCPConn::Close();

  return;
}

// Routine to issue SSL_write(3) on a open socket until everything has
// been written or the socket has become unavailable.
//
// Note, this routine can set an ErrorHandler event.
ssize_t SSLConn::Write(const char* buf, const ssize_t buf_len) {
  if (ssl_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Write(): ssl is NULL");
    return 0;
  }

  // SSL_write(3) will write *at most* buf_len into a SSL connection.  
  //
  // BLOCKING: SSL_write() will only return upon error, or completion 
  // of the write (i.e., buf_len was written to the SSL connection.) 
  // If renegotiation takes place, SSL_ERROR_WANT_READ will be 
  // returned.  However, if SSL_MODE_ENABLE_PARTIAL_WRITE is set,
  // then we can return without an ERROR is only part of the write
  // succeeds!
  // 
  // NONBLOCKING: SSL_write() will also return with SSL_ERROR_WANT_READ 
  // (or WANT_WRITE), when SSL_write() couldn't complete.  
  //
  // In either case, if SSL_write() has to be repeated because of 
  // WANT_READ or WANT_WRITE, the SSL_write() must be repeated with 
  // the same arguments!
  //
  // Note, re-negotiation can cause a SSL_ERROR_WANT_READ.

  if (!buf_len)
    return 0;	// nothing to write

  // Debugging:
  if (ERR_peek_error()) {
    _LOGGER(LOG_WARNING, "SSLConn::Write(): SSL error queue is non-empty: %s!",
            ssl_err_str().c_str());

    // Unfortunately, SSL_get_error() operates reliably only if the
    // error queue is empty.

    ERR_clear_error();
  }

  int bytes_wrote = SSL_write(ssl_, buf, buf_len);
  if (bytes_wrote == 0) {
    // From SSL_write(3): The write operation was not
    // successful. Probably the underlying connection was closed. Call
    // SSL_get_error() with the return value ret to find out, whether
    // an error occurred or the connection was shut down cleanly
    // (SSL_ERROR_ZERO_RETURN).

    switch(SSL_get_error(ssl_, bytes_wrote)) {
      case SSL_ERROR_ZERO_RETURN :
        {
          _LOGGER(LOG_WARNING, "SSLConn::Write(): %s unexpectedly sent \'close notify\' from %s.",
                  hostname().c_str());
          Shutdown(0);  // send our close notify
        }
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          _LOGGER(LOG_WARNING, "Received EOF from %s.", hostname().c_str());
          SSL_set_shutdown(ssl_, SSL_SENT_SHUTDOWN);  // mark the SSL connection as closed
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Write(): Received SSL_ERROR_SYSCALL: "
                     "%s terminated connection: %s", 
                     hostname().c_str(), ssl_err_str().c_str());
          return 0;
        }
        break;

      case SSL_ERROR_SSL :
        _LOGGER(LOG_WARNING, "SSLConn::Write(): Received SSL_ERROR_SSL: "
                "%s terminated connection: %s", hostname().c_str(), ssl_err_str().c_str());
        break;

      default:
        {
          error.Init(EX_SOFTWARE, "SSLConn::Write(): returned 0, unknown ERROR: %s", 
                     ssl_err_str().c_str());
          return 0;
        }
    }  // switch(SSL_get_error(ssl_, ret)) {
  } else if (bytes_wrote < 0) {
    // Check the ERROR condition ...
    switch(SSL_get_error(ssl_, bytes_wrote)) {
      case SSL_ERROR_ZERO_RETURN :  // TODO(aka) I don't think this is possible if ret < 0
        {
          error.Init(EX_SOFTWARE, "SSLConn::Write(): SSL_ERROR_ZERO_RETURN: "
                     "%s terminated connection", hostname().c_str());
          return bytes_wrote;
        }
        break;

      case SSL_ERROR_WANT_READ :
        if (IsBlocking()) {
          // See SSL_MODE_AUTO_RETRY in SSL_CTX_set_mode(3) for suggestions.
          _LOGGER(LOG_WARNING, "SSLConn::Write() received SSL_ERROR_WANT_READ "
                  "on blocking connection to %s (fd %d)",
                  hostname().c_str(), fd());
        } else {
          _LOGGER(LOG_INFO, "SSLConn::Write() received SSL_ERROR_WANT_READ "
                  "on non-blocking connection to %s on fd %d.",
                  hostname().c_str(), fd());
        }
        break;

      case SSL_ERROR_WANT_WRITE :
        if (IsBlocking()) {
          error.Init(EX_SOFTWARE, "SSLConn::Write() SSL_ERROR_WANT_WRITE: "
                     "on blocking connection to %s on fd %d",
                     hostname().c_str(), fd());
          return bytes_wrote;
        } else {
          // See SSL_MODE_AUTO_RETRY in SSL_CTX_set_mode(3) for suggestions.
          _LOGGER(LOG_INFO, "SSLConn::Write(): "
                  "Received SSL_ERROR_WANT_WRITE, returning.");
        }
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          error.Init(EX_SOFTWARE, "SSLConn::Write() SSL_ERROR_SYSCALL: "
                     "I/O error with %s on fd %d: %s", 
                     hostname().c_str(), fd(), strerror(errno));
          return bytes_wrote;
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Write() SSL_ERROR_SYSCALL: %s: %s", 
                     hostname().c_str(), ssl_err_str().c_str());
          return bytes_wrote;
        }
        break;

      case SSL_ERROR_SSL :
        {
          error.Init(EX_SOFTWARE, "SSLConn::Write() SSL_ERROR_SSL: %s: %s", 
                     hostname().c_str(), ssl_err_str().c_str());
          return bytes_wrote;
        }
        break;

      default:
        {
          error.Init(EX_SOFTWARE, "SSLConn::Write() unknown ERROR: %s", ssl_err_str().c_str());
          return bytes_wrote;
        }
    }  // switch(SSL_get_error(ssl_, ret)) {
  }  // else if (bytes_wrote < 0)

  _LOGGER(LOG_DEBUG, "SSLConn::Write(): Wrote %d byte(s) to: %s.", 
          bytes_wrote, print().c_str());

  return bytes_wrote;
}	

// This routine calls SSL_read(3) on an ready socket.	
//
// Note, this routine can set an ErrorHandler event.
//  
// TODO(aka) It might be cool if we overload Read() to include a
// version that uses a STL::string (or STL::data?) parameter instead
// of char*, in-order to be able to resize the buffer while in this
// routine ...
ssize_t SSLConn::Read(const ssize_t buf_len, char* buf, bool* eof) {
  if (ssl_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLConn::Read(): ssl is NULL");
    return 0;
  }

  // SSL_read(3) will read *at most* one SSL record.  
  //
  // Blocking: SSL_read() will only return upon error, or completion
  // of the read (i.e., the SSL record or buf_len of an SSL record if
  // buf_len < the SSL record.)  If renegotiation takes place,
  // SSL_ERROR_WANT_READ will be returned.
  // 
  // Non-Blocking: SSL_read() will also return with SSL_ERROR_WANT_READ 
  // (or WANT_WRITE), when SSL_read() couldn't complete.  
  //
  // In either case, if SSL_read() has to be repeated because of 
  // WANT_READ or WANT_WRITE, the SSL_read() must be repeated with 
  // the same arguments!
  //
  // Note that re-negotiation can cause a SSL_ERROR_WANT_WRITE!

#if 0  // XXX
  // Out of curiosity, see if we have any data pending in SSL.
  if (! (bytes_left = SSL_pending(ssl))) {
    _LOGGER(LOG_DEBUG, "No SSL data pending, requesting via SSL_read(3).");
  }
#endif

  // Debugging:
  if (ERR_peek_error()) {
    // TODO(aka): Because this is firing, we probably should use
    // ERR_get_error_line(3), or ERR_get_line_data(3) to see exactly
    // where these (missed?) errors are being generated!

    _LOGGER(LOG_WARNING, "SSLConn::Read(): SSL error queue is non-empty: %s.",
            ssl_err_str().c_str());

    // SSL_get_error() operates reliably only if the error queue 
    // is empty.

    ERR_clear_error();
  }

  *eof = false;
  int bytes_read = SSL_read(ssl_, buf, buf_len);

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: SSLConn::Read(): SSL_read() returned %db.", 
          bytes_read);
#endif

  if (bytes_read == 0) {
    // From SSL_read(3): The read operation was not successful. The
    // reason may either be a clean shutdown due to a "close notify"
    // alert sent by the peer (in which case the SSL_RECEIVED_SHUTDOWN
    // flag in the ssl shutdown state is set (see SSL_shutdown(3),
    // SSL_set_shutdown(3)). It is also possible, that the peer simply
    // shut down the underlying transport and the shutdown is
    // incomplete. Call SSL_get_error() with the return value ret to
    // find out, whether an error occurred or the connection was shut
    // down cleanly (SSL_ERROR_ZERO_RETURN).

    // Check for an ERROR condition ...
    switch(SSL_get_error(ssl_, bytes_read)) {
      case SSL_ERROR_ZERO_RETURN :
        _LOGGER(LOG_INFO, "SSLConn::Read(): "
                "Received \'close notify\' from %s on %d.", 
                hostname().c_str(), fd());
        Shutdown(0);  // send our close_notify
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          *eof = true;  // we got EOF
          _LOGGER(LOG_WARNING, "SSLConn::Read(): Received EOF from %s.",
                  hostname().c_str());
          SSL_set_shutdown(ssl_, SSL_SENT_SHUTDOWN);  // mark the SSL connection as closed
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Read(): Received SSL_ERROR_SYSCALL: "
                     "%s terminated connection: %s", 
                     hostname().c_str(), ssl_err_str().c_str());
          return 0;
        }
        break;  // not reached

      case SSL_ERROR_SSL :
        _LOGGER(LOG_WARNING, "SSLConn::Read(): Received SSL_ERROR_SSL: "
                "%s terminated connection: %s",
                hostname().c_str(), ssl_err_str().c_str());
        break;

      default:
        {
          error.Init(EX_SOFTWARE, "SSLConn::Read(): returned 0, unknown ERROR: %s", 
                     ssl_err_str().c_str());
          return 0;
        }
    }
  } else if (bytes_read < 0) {	// SSL_read() ERROR
    // Check the ERROR condition ...
    switch(SSL_get_error(ssl_, bytes_read)) {
      case SSL_ERROR_ZERO_RETURN :  // TODO(aka) I don't think this is possible if ret < 0
        {
          error.Init(EX_SOFTWARE, "SSLConn::Read(): SSL_ERROR_ZERO_RETURN: "
                     "%s terminated connection", hostname().c_str());
          return bytes_read;
        }
        break;

      case SSL_ERROR_WANT_READ :
        if (IsBlocking()) {
          // See SSL_MODE_AUTO_RETRY in SSL_CTX_set_mode(3) for suggestions.
          _LOGGER(LOG_WARNING, "SSLConn::Read(): received SSL_ERROR_WANT_READ "
                  "on blocking connection to %s (fd %d)",
                  hostname().c_str(), fd());
        } else {
          _LOGGER(LOG_INFO, "SSLConn::Read(): received SSL_ERROR_WANT_READ "
                  "on non-blocking connection to %s on fd %d.",
                  hostname().c_str(), fd());
        }
        break;

      case SSL_ERROR_WANT_WRITE :
        if (IsBlocking()) {
          error.Init(EX_SOFTWARE, "SSLConn::Read(): SSL_ERROR_WANT_WRITE: "
                     "on blocking connection to %s on fd %d",
                     hostname().c_str(), fd());
          return bytes_read;
        } else {
          // See SSL_MODE_AUTO_RETRY in SSL_CTX_set_mode(3) for suggestions.
          _LOGGER(LOG_INFO, "SSLConn::Read(): "
                  "Received SSL_ERROR_WANT_WRITE, returning.");
        }
        break;

      case SSL_ERROR_SYSCALL :
        // From SSL_get_error(3): Some I/O error occurred.  The
        // OpenSSL error queue may contain more information on the
        // error.  If the error queue is empty (i.e. ERR_get_error()
        // returns 0), ret can be used to find out more about the
        // error: If ret == 0, an EOF was observed that violates the
        // protocol.  If ret == -1, the underlying BIO reported an I/O
        // error (for socket I/O on Unix systems, consult errno for
        // details).

        if (!ERR_peek_error()) {
          error.Init(EX_SOFTWARE, "SSLConn::Read(): SSL_ERROR_SYSCALL: "
                     "I/O error with %s on fd %d: %s",
                     hostname().c_str(), fd(), strerror(errno));
          return bytes_read;
        } else {
          error.Init(EX_SOFTWARE, "SSLConn::Read(): SSL_ERROR_SYSCALL: %s: %s", 
                     hostname().c_str(), ssl_err_str().c_str());
          return bytes_read;
        }
        break;

      case SSL_ERROR_SSL :
        {
          error.Init(EX_SOFTWARE, "SSLConn::Read(): SSL_ERROR_SSL: %s: %s", 
                     hostname().c_str(), ssl_err_str().c_str());
          return bytes_read;
        }
        break;

      default:
        {
          if (IsBlocking())
            error.Init(EX_SOFTWARE, "SSLConn::Read(): "
                       "unknown blocking ERROR: %s", ssl_err_str().c_str());
          else
            error.Init(EX_SOFTWARE, "SSLConn::Read(): "
                       "unknown non-blocking ERROR: %s", ssl_err_str().c_str());
          return bytes_read;
        }
    }  // switch(SSL_get_error(ssl_, ret)) {
  }  // else if (bytes_read < 0)

  // For Debugging:
#if 0
  bytes_left = SSL_pending(ssl_);  // out of curiosity
#endif

  if (*eof)
    _LOGGER(LOG_DEBUG, "SSLConn::Read(): Read EOF from: %s.", print().c_str());

  if (bytes_read) {
    _LOGGER(LOG_DEBUG, "SSLConn::Read(): Read %d byte(s) from: %s.", 
            bytes_read, print().c_str());
  }

  return bytes_read;
}

#if 0  // TODO(aka) The following two calls are deprecated.

// This routine calls read(2) on an ready socket, until it either
// returns with an ERROR or EOF.
//
// Note, this routine can set an ErrorHandler event.
ssize_t SSLConn::ReadExhaustive(const ssize_t buf_len, char* buf, 
                                bool* eof) {
  
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
    error.Init(EX_SOFTWARE, "SSLConn::Read(): routine called on blocking socket");
    return 0;
  }

  *eof = false;

  ssize_t bytes_read = 0;
  ssize_t n = 0;
  ssize_t bytes_left = buf_len;
  off_t offset = 0;
  for (;;) {
    // TODO(aka) This should call SSLConn::Read() to get the work done!
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
        error.Init(EX_IOERR, "SSLConn::Read(): failed: %s", 
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
ssize_t SSLConn::ReadLine(const char delimiter, const ssize_t buf_len, 
                          char* buf, bool* eof) {
  //int SSL_Conn::Read_Record(String& str, size_t offset)

	// If we wanted to be cool, we could use "transparent negotiation",
	// i.e., we wouldn't need SSL_connect() or SSL_accept(), but we
	// *would* need to explicitly put the box in server or client mode :-(

	if (! ssl)
		 error.Init(EX_SOFTWARE, "SSLConn::Read_Record(): "
					"ssl is NULL");

	// SSL_read(3) will read *at most* one SSL record.  
	//
	// Blocking: SSL_read() will only return upon error, or completion 
	// of the read (i.e., the SSL record or buf_len of an SSL record 
	// if buf_len < the SSL record.)  If renegotiation takes place, 
	// SSL_ERROR_WANT_READ will be returned.
	// 
	// Non-Blocking: SSL_read() will also return with SSL_ERROR_WANT_READ 
	// (or WANT_WRITE), when SSL_read() couldn't complete.  
	//
	// In either case, if SSL_read() has to be repeated because of 
	// WANT_READ or WANT_WRITE, the SSL_read() must be repeated with 
	// the same arguments!
	//
	// Note that re-negotiation can cause a SSL_ERROR_WANT_WRITE!

	// Since it's possible that the SSL_read() call in SSLConn::Read()
	// can free up *more* data in the SSL record then can be read with
	// our current buffer size, we loop here until bytes_left is 0.

	int bytes_read = 0;
	while (1) {
		// If we have data already in SSL, resize now to accept it.
		int ssl_buf_size = SSL_pending(ssl);	// get record size

		if (ssl_buf_size > (int) (str.Buf_Size() - offset)) {
                  logger.Log(LOG_DEBUGGING, "Read_Record(): pending = %d, current = %d, offset = %d, resizing buffer to %d.", ssl_buf_size, str.Buf_Size(), offset, str.Buf_Size() + ((size_t) ssl_buf_size - (str.Buf_Size() - offset)) + 1);

			str.Resize(str.Buf_Size() + ((size_t) ssl_buf_size - 
						     (str.Buf_Size() - offset))
				   + 1);
		}

		// Call SSLConn::Read(), which can throw exception ...
		int bytes_left = 0;
		int n = Read(str.Get_Buf(), str.Buf_Size(), offset, 
			     bytes_left);
		bytes_read += n;
		offset += n;

		// logger.Log(LOG_DEBUGGING, "Read_Record(): After read, n = %d, bytes read = %d, bytes left = %d", n, bytes_read, bytes_left);

		if (n <= 0 && bytes_left)
			 error.Init(EX_SOFTWARE, "SSLConn::Read_Record(): "
						"Hmm, %d byte(s) of data "
						"pending, but SSL_read(3) "
						"returned: %d",
						bytes_left, n);
		
		if (! bytes_left)
			break;
	}

	return bytes_read;



	
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
        _LOGGER(LOG_DEBUG, "SSLConn::ReadLine(): null at byte %d.", bytes_read);
        break;  // don't increment ptr or counter
      } else if (c == '\r') {
        _LOGGER(LOG_DEBUG, "SSLConn::ReadLine(): \\r at byte %d.", bytes_read);

        // TODO(aka) Why do I need this?  If we want to break of 
        break;  // don't increment ptr or counter
      } else if (c == delimiter) {
        _LOGGER(LOG_DEBUG, "SSLConn::ReadLine(): delimiter at byte %d.", bytes_read);

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
      error.Init(EX_IOERR, "SSLConn::ReadLine(): read(fd: %d): %s",
                 descriptor_->fd_, strerror(errno));
      return 0;
    }
  }

  *ptr = '\0';

  if (bytes_read) {
    _LOGGER(LOG_INFO, "Read %d byte(s) from: %s.",
            bytes_read, hostname().c_str());

    _LOGGER(LOG_DEBUG, "SSLConn::ReadLine(): read: %s.", buf);
  }

  return bytes_read;
}
#endif

// Boolean functions.

// Routine to see if someone has attempted to shudown the SSL connection.
const int SSLConn::IsShutdownInitiated(void) const {
  int state = SSL_get_shutdown(ssl_);
  return ((state & SSL_SENT_SHUTDOWN || state & SSL_RECEIVED_SHUTDOWN) ? 1 : 0);
}

// Routine to see if the SSL connection is shutdown.
const int SSLConn::IsShutdownComplete(void) const {
  int state = SSL_get_shutdown(ssl_);
  return ((state & SSL_SENT_SHUTDOWN && state & SSL_RECEIVED_SHUTDOWN) ? 1 : 0);
}

#if 0  // XXX
bool SSLConn::Equals(const SSLConn& other) const {
  // Use SSLConn::operator ==() to get the job done.
  if (SSLConn::operator ==(other) == 0)
    return false;

  return true;
}
#endif

// Debugging stuff functions.


// Obsoleted.
#if 0
// TODO(aka) I'm 99% sure that we do not need these ...
SSLConn::SSLConn(const in_port_t port, const in_addr_t host_in_addr,
                 const int family)
    : IPComm(host_in_addr, family) {
  sock_addr_.sin_port = htons(port);
  connected_ = true;
  listen;
}

SSLConn::SSLConn(const in_port_t port, const char* host, const int family)
    : IPComm(host, family) {
  sock_addr_.sin_port = htons(port);
  connected_ = true;
  listen;
}
#endif

