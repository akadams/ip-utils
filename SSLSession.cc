/* $Id: SSLSession.cc,v 1.7 2014/04/11 17:42:15 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>        // for lseek(2)

#include "ErrorHandler.h"
#include "Logger.h"

#include "SSLSession.h"

#define DEBUG_CLASS 0
#define DEBUG_INCOMING_DATA 0
#define DEBUG_OUTGOING_DATA 0
#define DEBUG_MUTEX_LOCK 0

// Non-class specific defines & data structures.
const size_t kDefaultBufSize = 4096;

uint16_t unique_session_id = 65;  // start at least 6 bits over

// Non-class specific utilities.

// Template Class.

// Constructors and destructor.
SSLSession::SSLSession(const uint8_t framing_type)
    : rfile_(), rhdr_(framing_type), wfiles_(), wpending_(), whdrs_() {
#if DEBUG_CLASS
  warnx("SSLSession::SSLSession(void) called.");
#endif

  framing_type_ = framing_type;
  handle_ = unique_session_id++;
  synchronize_connection_ = false;
  synchronize_status_ = 0;
  rbuf_ = NULL;
  rbuf_size_ = 0;
  rbuf_len_ = 0;
  memset(&rpending_, 0, sizeof(rpending_));
  rpending_.storage_initialized = false;
  rtid_ = TCPSESSION_THREAD_NULL;
  wbuf_ = NULL;
  wbuf_size_ = 0;
  wbuf_len_ = 0;

  pthread_mutex_init(&incoming_mtx, NULL);
  pthread_mutex_init(&outgoing_mtx, NULL);
}

SSLSession::~SSLSession(void) {
#if DEBUG_CLASS
  warnx("SSLSession::~SSLSession(void) called.");
#endif

  if (rbuf_)
    free((void*)rbuf_);
  if (wbuf_)
    free((void*)wbuf_);

  pthread_mutex_destroy(&incoming_mtx);
  pthread_mutex_destroy(&outgoing_mtx);
}

// Copy constructor, assignment and equality operator needed for STL.
SSLSession::SSLSession(const SSLSession& src)
    : SSLConn(src), rfile_(src.rfile_), rhdr_(src.rhdr_), 
      wfiles_(src.wfiles_), wpending_(src.wpending_), whdrs_(src.whdrs_) {
#if DEBUG_CLASS
  warnx("SSLSession::SSLSession(const SSLSession&) called.");
#endif

  rbuf_ = NULL;
  rbuf_size_ = 0;
  rbuf_len_ = 0;
  wbuf_ = NULL;
  wbuf_size_ = 0;
  wbuf_len_ = 0;

  // Note, usaully one must call SSLSession::Init() to generate
  // buffers (as we allow Init() to set ErrorHandler events, however,
  // we don't have that luxury here.  Hence, we must handle errors in
  // here, which may prove to be difficult.  Damn my desire to put
  // networking objects in the STL!

  if (src.rbuf_size_ > 0) {
    if ((rbuf_ = (char*)malloc(src.rbuf_size_)) == NULL) {
      _LOGGER(LOG_ERR, "SSLSession(const SSLSession& src): malloc(%ld) failed",
              src.rbuf_size_);
      return;
    }
  }
  if (src.wbuf_size_ > 0) {
    if ((wbuf_ = (char*)malloc(src.wbuf_size_)) == NULL) {
      _LOGGER(LOG_ERR, "SSLSession(const SSLSession& src): malloc(%ld) failed",
              src.wbuf_size_);
      return;
    } 
  }

  // If we made it here, our mallocs worked, so set the rest of the object.
  framing_type_ = src.framing_type_;
  handle_ = src.handle_;
  synchronize_connection_ = src.synchronize_connection_;
  synchronize_status_ = src.synchronize_status_;

  rbuf_size_ = src.rbuf_size_;

  if (src.rbuf_len_) {
    memcpy(rbuf_, src.rbuf_, src.rbuf_len_);
    rbuf_len_ = src.rbuf_len_;
  }

  memcpy(&rpending_, &src.rpending_, sizeof(rpending_));
  rhdr_ = src.rhdr_;
  rtid_ = src.rtid_;

  wbuf_size_ = src.wbuf_size_;

  if (src.wbuf_len_) {
    memcpy(wbuf_, src.wbuf_, src.wbuf_len_);
    wbuf_len_ = src.wbuf_len_;
  }

  // Copies get their own MUTEXs.
  pthread_mutex_init(&incoming_mtx, NULL);
  pthread_mutex_init(&outgoing_mtx, NULL);
}

#if 0  // XXX
SSLSession::operator =(const SSLSession& src) {
#if DEBUG_CLASS
  warnx("SSLSession::operator =(const SSLSession&) called.");
#endif

}
#endif

#if 0  // XXX
int SSLSession::operator ==(const SSLSession& other) const {
  if (SSLConn::operator ==(other) == 0)
    return 0;  // if SSLConn::operator ==() is false, we're all done

  // If our id, buffer sizes and data lengths match, most likely we're
  // are the same.

  if (handle_ == other.handle_ && rbuf_size_ == other.rbuf_size_ 
      && rfile_ == other.rfile_ &&
      wbuf_size_ == other.wbuf_size_ && rbuf_len_ == other.rbuf_len_ &&
      wbuf_len_ == other.wbuf_len_ &&
      wfiles_.size() == other.wfiles_.size() &&
      wpending_.size() == other.wpending_.size())
    return 1;

  return 0;
}
#endif

// Accessors.

// Mutators.
void SSLSession::set_handle(const uint16_t handle) { 
  handle_ = handle; 
}

void SSLSession::set_synchronize_status(const uint8_t synchronize_status) {
  synchronize_status_ = synchronize_status;
}

// Wrapper function for TCPConn::set_connected().  This is needed,
// because prior to calling TCPConn::set_connected(), we need to make
// sure we have both locks (which TCPConn knows nothing about).
void SSLSession::set_connected(const bool connected) {
#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_connected(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  // Note, multiple locking order is: incoming, than outgoing!
#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_connected(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  TCPConn::set_connected(connected);

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_connected(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_connected(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to setup the streaming *read* File object.
//
// Note, this routine can set an ErrorHandler event.
void SSLSession::set_rfile(const char* path, const ssize_t len) {
  if (path == NULL || strlen(path) == 0) {
    error.Init(EX_SOFTWARE, "SSLSession::set_rfile(): path is NULL or empty");
    return;
  }

  if (rfile_.IsOpen()) {
    error.Init(EX_SOFTWARE, "SSLSession::set_rfile(): rfile is open");
    return;
  }

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_rfile(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  if (!rfile_.name().empty()) {
    rfile_.clear();
  }

  rpending_.file_offset = 0;
  rpending_.storage = SESSION_USE_DISC;
  rpending_.storage_initialized = true;

  rfile_.InitFromBuf(path, len);

  // TOOD(aka) Do we want O_APPEND here?
  rfile_.Open(NULL, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
  if (error.Event())
    error.AppendMsg("SSLSession::set_rfile(): ");

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_rfile(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to set the Incoming Message thead id.
void SSLSession::set_rtid(const pthread_t rtid) {
#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_rtid(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  rtid_ = rtid;

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_rtid(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

void SSLSession::set_storage(const int storage) {
#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_storage(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  rpending_.storage = storage;
  rpending_.storage_initialized = true;

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::set_storage(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to *erase* a specific MsgHdr from our list (whdrs_).
void SSLSession::delete_whdr(list<MsgHdr>::iterator msg_hdr) {
#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::delete_whdr(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  whdrs_.erase(msg_hdr);

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::delete_whdr(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
}


// SSLSession manipulation.

// Routine to *pretty* print object.
string SSLSession::print(void) const {
  // Print retuns the host, port and socket.
  string tmp_str(kDefaultBufSize, '\0');

  snprintf((char*)tmp_str.c_str(), kDefaultBufSize, 
           "%s:%d:%lu:%ld:%ld:%s:%ld:%ld:%ld:%ld:%ld", 
           SSLConn::print().c_str(), handle_, rtid_, (long)rbuf_size_, (long)rbuf_len_, 
           rfile_.print().c_str(), (long)wbuf_size_, (long)wbuf_len_,
           (long)wfiles_.size(), (long)wpending_.size(), (long)whdrs_.size());


  return tmp_str;
}

void SSLSession::Init(void) {
  if (rbuf_ != NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::Init(): rbuf is not NULL");
    return;
  }

  if (rtid_ != TCPSESSION_THREAD_NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::Init(): rtid is not NULL");
    return;
  }

  if ((rbuf_ = (char*)malloc(kDefaultBufSize)) == NULL) {
    error.Init(EX_OSERR, "SSLSession::Init(): rbuf malloc(%d) failed", 
               kDefaultBufSize);
    return;
  }

  rbuf_size_ = kDefaultBufSize;
  rbuf_len_ = 0;

  if ((wbuf_ = (char*)malloc(kDefaultBufSize)) == NULL) {
    error.Init(EX_OSERR, "SSLSession::Init(): wbuf malloc(%d) failed", 
               kDefaultBufSize);
    return;
  }

  wbuf_size_ = kDefaultBufSize;
  wbuf_len_ = 0;
}

// Routine to setup or initialize the incoming message on a peer.  If
// we have aneough data, i.e., InitFromBuf() returns TRUE, we
// initialize our incoming message's meta-data (rpending_).
bool SSLSession::InitIncomingMsg(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::InitIncomingMsg(): rbuf is NULL");
    return false;
  }

  if (rbuf_len_ == 0)
    return false;  // nothing to do

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::InitIncomingMsg(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  if (!rhdr_.InitFromBuf(rbuf_, rbuf_len_)) {
    if (error.Event()) {
      error.AppendMsg("SSLSession::InitIncomingMsg(): ");
      ResetRbuf();
    }

#if DEBUG_MUTEX_LOCK
    warnx("SSLSession::InitIncomingMsg(): releasing incoming lock.");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return false;  // not enough data or ErrorHandler event
  }

  // If we made it here, we parsed the framing header, so remove the
  // framing header from our buffer.

  ShiftRbuf(rhdr_.hdr_len(), 0);

  // Build our SSLSession meta-data for the incoming message.
  rpending_.initialized = 1;
  rpending_.msg_id = rhdr_.msg_id();
  rpending_.hdr_len = rhdr_.hdr_len();
  rpending_.body_len = rhdr_.body_len();
  rpending_.buf_offset = 0;
  rpending_.file_offset = 0;
  rpending_.storage = SESSION_USE_MEM;  // default storage

  // Note, we do *not* mark the storage type as initialized, we just
  // set it to the default type!

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::InitIncomingMsg(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
  return true;
}

// Routine to install a new message (stored in memory) into our
// outgoing message queue.
//
// This routine can set an ErrorHandler event.
bool SSLSession::AddMsgBuf(const char* framing_hdr, const ssize_t hdr_len, 
                           const char* msg_body, const ssize_t body_len,
                           const MsgHdr& whdr) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::AddMsgBuf(): wbuf_ is NULL");
    return false;
  }

  if (framing_hdr == NULL || strlen(framing_hdr) == 0) {
    error.Init(EX_SOFTWARE, "SSLSession::AddMsgBuf(): "
               "framing_hdr is NULL or empty");
    return false;
  }

  // TODO(aka) The problem with this routine is that the four
  // components that make up a message (framing_hdr & msg_body in
  // wbuf_, whdr_ and wpending_) are *not* linked.  That is, we assume
  // that the next hdr & body in wbuf_ will match the next MsgInfo and
  // MsgHdr in wpending_ and whdrs_ respectively.  But I'm not sure
  // that's a safe assumption!

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::AddMsgBuf(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  // Make sure we have enough room for the msg header & body
  if ((hdr_len + body_len + wbuf_len_) > wbuf_size_) {
    _LOGGER(LOG_DEBUG, "SSLSession::AddMsgBuf(): reallocing wbuf_, msg_len (%ld) + current wlen (%ld) is greater than wbuf_size (%ld).", hdr_len + body_len, wbuf_len_, wbuf_size_);

    ssize_t new_wbuf_size = hdr_len + body_len + wbuf_len_ + kDefaultBufSize;
    char* p = (char*)realloc(wbuf_, new_wbuf_size);
    if (p == NULL) {
      // Set an ErrorHandler event and return.
      error.Init(EX_OSERR, "SSLSession::AddMsgBuf(): realloc(%ld) failed", 
                 new_wbuf_size);
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::AddMsgBuf(): releasing outgoing lock.");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return false;
    } else {
      wbuf_ = p;
      wbuf_size_ = new_wbuf_size;
    }
  }

  // Build our message's meta-data info and add to our queue (wpending_).
  struct MsgInfo msg_info;
  msg_info.storage = SESSION_USE_MEM;
  msg_info.storage_initialized = true;
  msg_info.msg_id = whdr.msg_id();  // give message unique id
  msg_info.hdr_len = hdr_len;  // mark the size of our message header
  msg_info.body_len = body_len;  // mark the size of our message body
  msg_info.buf_offset = 0;  // what we've sent so far
  msg_info.file_offset = 0;  // not used for this message
  wpending_.push_back(msg_info);

  // Install the message header and message body in our outgoing
  // buffer (wbuf_)

  memcpy(wbuf_ + wbuf_len_, framing_hdr, hdr_len);  // install the hdr
  wbuf_len_ += hdr_len;  // aggregate buffer length
  memcpy(wbuf_ + wbuf_len_, msg_body, body_len);  // install the body
  wbuf_len_ += body_len;  // aggregate buffer length

  // Finally, add a copy of our outgoing msg's framing header to
  // whdrs_, in-case we need to deal with a RESPONSE, i.e., we can
  // check what this message (if it was a REQUEST) was for when we
  // receive the RESPONSE.

  if (whdr.IsMsgRequest())
    whdrs_.push_back(whdr);

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::AddMsgBuf(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
  return true;
}

// Routine to install a new message (stored in memory) into our
// outgoing message queue.
bool SSLSession::AddMsgFile(const char* framing_hdr, const ssize_t hdr_len, 
                            const File& msg_body, const ssize_t body_len,
                            const MsgHdr& whdr) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::AddMsgFile(): wbuf is NULL");
    return false;
  }

  if (framing_hdr == NULL || strlen(framing_hdr) == 0) {
    error.Init(EX_SOFTWARE, "SSLSession::AddMsgFile(): "
               "framing_hdr is NULL or empty");
    return false;
  }

  // TODO(aka) The problem with this routine is that the four
  // components that make up a message (framing_hdr & msg_body in
  // wbuf_, whdr_ and wpending_) are *not* linked.  That is, we assume
  // that the next hdr & body in wbuf_ will match the next MsgInfo and
  // MsgHdr in wpending_ and whdrs_ respectively.  But I'm not sure
  // that's a safe assumption!

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::AddMsgFile(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  // Make sure we have enough room for the msg header & body.
  if (hdr_len + wbuf_len_ > wbuf_size_) {
    _LOGGER(LOG_DEBUG, "SSLSession::AddMsgFile(): reallocing wbuf_, hdr_len (%ld) + current wlen (%ld) is greater than wbuf_size (%ld).", hdr_len, wbuf_len_, wbuf_size_);

    ssize_t new_wbuf_size = hdr_len + wbuf_len_ + kDefaultBufSize;
    char* p = (char*)realloc(wbuf_, new_wbuf_size);
    if (p == NULL) {
      // Set an ErrorHandler event and return.
      error.Init(EX_OSERR, "SSLSession::AddMsgFile(): "
                 "realloc(%ld) failed", new_wbuf_size);
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::AddMsgFile(): releasing outgoing lock (-1).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return false;
    } else {
      wbuf_ = p;
      wbuf_size_ = new_wbuf_size;
    }
  }

  // Build our message's meta-data info and add to our queue (wpending_).
  struct MsgInfo msg_info;
  msg_info.storage = SESSION_USE_DISC;
  msg_info.storage_initialized = true;
  msg_info.msg_id = whdr.msg_id();  // give message unique id
  msg_info.hdr_len = hdr_len;  // mark the size of our message header
  msg_info.body_len = body_len;  // mark the size of our message body
  msg_info.buf_offset = 0;  // what we've sent so far
  msg_info.file_offset = 0;  // not used for this message
  wpending_.push_back(msg_info);

  // Install the message header in our outgoing buffer and the file in
  // wfiles_.

  memcpy(wbuf_ + wbuf_len_, framing_hdr, hdr_len);
  wbuf_len_ += hdr_len;  // aggregate buffer length

  // TODO(aka) file should be assigned either to an index (hard, as we
  // may skip indexes if the message is in wbuf_), or have an
  // associated key (or id) that can be looked up in wpending_.

  wfiles_.push_back(msg_body);

  // Finally, add a copy of our outgoing msg's framing header to
  // whdrs_, in-case we need to deal with a RESPONSE, i.e., we can
  // check what this message (if it was a REQUEST) was for when we
  // receive the RESPONSE.

  if (whdr.IsMsgRequest())
    whdrs_.push_back(whdr);

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::AddMsgFile(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
  return true;
}

// Routine to read, via SSLConn::Read(), any data on the socket and
// store in our internal buffer (rbuf_).
//
// Note, this routine can set an ErrorHandler event.
ssize_t SSLSession::Read(bool* eof) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::Read(): rbuf is NULL");
    return 0;
  }

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::Read(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  //_LOGGER(LOG_DEBUG, "SSLSession::Read(): calling Read(), session: %s.", print().c_str());

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::Read(): "
          "Entering: rpending msg: %d, %d, %ld, %ld, %ld, %ld.", 
          rpending_.storage, rpending_.storage_initialized, 
          rpending_.hdr_len, rpending_.body_len, rpending_.buf_offset,
          rpending_.file_offset);
#endif

  // Call SSLConn::Read() to get the work done.
  ssize_t bytes_read = 
      SSLConn::Read(rbuf_size_ - rbuf_len_, rbuf_ + rbuf_len_, eof);
  if (error.Event()) {
    error.AppendMsg("SSLSession::Read(): "
                    "rbuf_ %p, rbuf_len_ %ld, rbuf_size_ %ld, eof %d: "
                    "clearing rbuf_: ", 
                    rbuf_, rbuf_len_, rbuf_size_, *eof);
    ResetRbuf();
#if DEBUG_MUTEX_LOCK
    warnx("SSLSession::Read(): releasing incoming lock (-1).");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return 0;
  }

  rbuf_len_ += bytes_read;

  // Resize rbuf_ if we're out of room.
  if (rbuf_len_ == rbuf_size_) {
    _LOGGER(LOG_DEBUG, "SSLSession::Read(): reallocing rbuf_"
            ", rbuf_len %ld, rbuf_size %ld.",
            rbuf_len_, rbuf_size_);
    ssize_t new_rbuf_size = rbuf_size_ + kDefaultBufSize;
    char* p = (char*)realloc(rbuf_, new_rbuf_size);
    if (p == NULL) {
      // Set an ErrorHandler event and return.
      error.Init(EX_OSERR, "SSLSession::Read(): realloc(%ld) failed", 
                 new_rbuf_size);
      ResetRbuf();
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::Read(): releasing incoming lock (-2).");
#endif
      pthread_mutex_unlock(&incoming_mtx);
      return 0;
    } else {
      rbuf_ = p;
      rbuf_size_ = new_rbuf_size;
    }
  }

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::Read(): "
          "Leaving: rpending msg: %d, %ld, %ld, %ld, %ld.", 
          rpending_.storage, rpending_.hdr_len, rpending_.body_len, 
          rpending_.buf_offset, rpending_.file_offset);
#endif

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::Read(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
  return bytes_read;
}

// Routine to write, via SSLConn::Write(), the next messages in our
// object.  The message info (struct MsgInfo) queue (wpending_)
// contains the meta data for each message to be sent out.  The
// message header is always in the internal memory buffer (wbuf_),
// until the entire message has been sent.  However, the body of the
// message can either also be in wbuf_ or it can be in the File object
// queue (wfiles_).  Offsets for each data location allow for us to
// work on chunks of data (for each call of this routine).
//
// Since we do *not* cleanup after we have finished sending the
// message in here, we need to make sure that we call our
// outgoing_msg() check routine right after event_pollout().
//
// Note, this routine can set an ErrorHandler event.
ssize_t SSLSession::Write(void) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::Write(): wbuf is NULL");
    return 0;
  }

  if (wpending_.size() == 0) {
    error.Init(EX_SOFTWARE, "SSLSession::Write(): wpending is empty");
    return 0;
  }

  if (!IsConnected()) {
    error.Init(EX_SOFTWARE, "SSLSession::Write(): not connected");
    return 0;
  }

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::Write(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::Write(): "
            "Enter: pending msg: %d, %ld, %ld, %ld, %ld.", 
            wpending_.front().storage, wpending_.front().hdr_len, 
            wpending_.front().body_len, wpending_.front().buf_offset, 
            wpending_.front().file_offset);
  else
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::Write(): "
            "Enter: pending msg NULL.");
#endif

  // For convenience, make copies of readonly variables.
  const ssize_t hdr_len = wpending_.front().hdr_len;
  const ssize_t body_len = wpending_.front().body_len;

  ssize_t bytes_sent = 0;

  if (wpending_.front().storage == SESSION_USE_MEM) {
    // Message in within the internal memory buffer (wbuf_).

    //_LOGGER(LOG_DEBUG, "SSLSession::Write(): lengths_ is of size %d.", lengths_.size());

    ssize_t msg_len = hdr_len + body_len;  // message is all within wbuf_

    // Send the message out (header & body) using SSLConn::Write().
    bytes_sent = 
        SSLConn::Write(wbuf_ + wpending_.front().buf_offset, 
                       msg_len - wpending_.front().buf_offset);
    if (error.Event()) {
      error.AppendMsg("SSLSession::Write(): "
                      "wbuf_ %p, wbuf_len_ %ld, wbuf_size_ %ld"
                      ", msg_len %ld, offset %ld: ", 
                      wbuf_, wbuf_len_, wbuf_size_, msg_len,
                      wpending_.front().buf_offset);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::Write(): releasing outgoing lock (1).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return 0;
    }

    wpending_.front().buf_offset += bytes_sent;  // update offset in queue

    if ((bytes_sent + wpending_.front().buf_offset) >= msg_len) {
      // Yeah, we sent all the data!

      //_LOGGER(LOG_DEBUG, "SSLSession::Write(): Should I remove %ld byte message from wbuf, new len(%ld), cnt (%d).", msg_len, wbuf_len_, wpending_.size());
    } 
  } else {
    // Message is split between internal memory buffer (header is in
    // wbuf_) and the File object queue (body is in wfiles_).

    // First, see if the header needs to be sent.
    if (wpending_.front().buf_offset < hdr_len) {
      // Send the message header out using SSLConn::Write().
      bytes_sent = 
          SSLConn::Write(wbuf_ + wpending_.front().buf_offset,
                         hdr_len - wpending_.front().buf_offset);
      if (error.Event()) {
        error.AppendMsg("SSLSession::Write(): "
                        "wbuf_ %p, wbuf_len_ %ld, wbuf_size_ %ld"
                        ", hdr_len %ld, offset %ld: ", 
                        wbuf_, wbuf_len_, wbuf_size_, hdr_len,
                        wpending_.front().buf_offset);
        ResetWbuf();
#if DEBUG_MUTEX_LOCK
        warnx("SSLSession::Write(): releasing outgoing lock (2).");
#endif
        pthread_mutex_unlock(&outgoing_mtx);
        return 0;
      }
      
      wpending_.front().buf_offset += bytes_sent;  // update offset in queue
    }

    if (wpending_.front().buf_offset < hdr_len) {
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::Write(): releasing outgoing lock (3).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return bytes_sent;  // we were unable to send entire header, so return
    }

    // Okay, if we made it here, we know the header was sent, so we
    // can send (some of) the File object out.

    char tmp_buf[kFileChunkSize];  // setup copy buffer 

    // If the file isn't open, open it (and seek).
    if (!wfiles_.front().IsOpen()) {
      wfiles_.front().Open(NULL, O_RDONLY, 0);
      if (error.Event()) {
        error.AppendMsg("SSLSession::Write(): current offset %ld: ", 
                        wpending_.front().file_offset);
        ResetWbuf();
#if DEBUG_MUTEX_LOCK
        warnx("SSLSession::Write(): releasing outgoing lock (4).");
#endif
        pthread_mutex_unlock(&outgoing_mtx);
        return 0;
      }
      
      // Since we needed to open it, let's jump to the offset.
      if (lseek(wfiles_.front().fd(), wpending_.front().file_offset, 
                SEEK_SET) < 0) {
        error.Init(EX_IOERR, "SSLSession::Write(): lseek(%s) failed: %s",
                   wfiles_.front().print().c_str(), strerror(errno));
        ResetWbuf();
#if DEBUG_MUTEX_LOCK
        warnx("SSLSession::Write(): releasing outgoing lock (5).");
#endif
        pthread_mutex_unlock(&outgoing_mtx);
        return 0;
      }
    }
      
    // Read the next chunk of data from the file ...
    size_t read_amount = (kFileChunkSize < 
                          (body_len - wpending_.front().file_offset)) ?
        kFileChunkSize : (body_len - wpending_.front().file_offset);
    ssize_t n = read(wfiles_.front().fd(), tmp_buf, read_amount);
    if (n == 0) {
      // EOF
      error.Init(EX_IOERR, "SSLSession::Write(): TOOD(aka) "
                 "read EOF from %s, but file_offset is %ld, size is %ld",
                 wfiles_.front().print().c_str(), 
                 wpending_.front().file_offset, body_len);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::Write(): releasing outgoing lock (6).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return bytes_sent;
    } else if (n < 0) {
      // ERROR
      error.Init(EX_IOERR, "SSLSession::Write(): read(%s) failed"
                 ", file_offset is %ld, size is %ld",
                 wfiles_.front().print().c_str(),
                 wpending_.front().file_offset, body_len);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::Write(): releasing outgoing lock (7).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return bytes_sent;
    }

    // ... and send the next chunk out using SSLConn::Write()
    bytes_sent = SSLConn::Write(tmp_buf, n);  // redo bytes_sent
    if (error.Event()) {
      error.AppendMsg("SSLSession::Write(): "
                      "file %s, body_len %ld, file_offset %ld: ", 
                      wfiles_.front().print().c_str(), body_len,
                      wpending_.front().file_offset);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("SSLSession::Write(): releasing outgoing lock (8).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return 0;
    }

    wpending_.front().file_offset += bytes_sent;  // update offset in queue

    if (wpending_.front().file_offset >= body_len) {
      // Yeah, we sent all the data out, let's clean up.

      // TODO(aka) Should we clean up here?  The Probably not, as we
      // can do a better job back up in tcp_event_foo() or in the main
      // event-loop!  Note, even if we decided to clean-up here, what
      // would we clean up?

      /*
      printf("SSLSession::Write(): DEBUG: memmoving %ld bytes at %p to %p.\n", wbuf_len_ - msg_len, wbuf_ + msg_len, wbuf_);
      memmove(wbuf_, wbuf_ + msg_len, wbuf_len_ - msg_len);
      wbuf_len_ -= msg_len;
      wpending_.pop();

      _LOGGER(LOG_DEBUG, "SSLSession::Write(): removed %ld byte message from wbuf, new len(%ld), cnt (%d).", msg_len, wbuf_len_, wpending_.size());
      */
      //_LOGGER(LOG_DEBUG, "SSLSession::Write(): Should I remove %ld byte message from wfiles, new len(%ld), cnt (%d).", body_len, wbuf_len_, wpending_.size());
    }
  }  // else if (wpending_.front().storage == SESSION_USE_MEM) {

#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::Write(): "
            "Leaving: pending msg: %d, %ld, %ld, %ld, %ld.",
            wpending_.front().storage, wpending_.front().hdr_len,
            wpending_.front().body_len, wpending_.front().buf_offset, 
            wpending_.front().file_offset);
  else
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::Write(): "
            "Leaving: pending msg NULL.");
#endif

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::Write(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);

  return bytes_sent;
}

// Routine to move data in our internal read buffer (rbuf_) to a file.
// If we've read the entire file, we close the file and return 0.
// Remember that the header should *not* still be in the buffer (as we
// *must* have processed it by now in-order to know how much data to
// read!).
//
// Note, this routine can set an ErrorHandler event.
int SSLSession::StreamIncomingMsg(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::StreamIncomingMsg(): rbuf is NULL");
    return 1;
  }

  if (!rfile_.IsOpen()) {
    error.Init(EX_SOFTWARE, "SSLSession::StreamIncomingMsg(): rfile not open");
    return 1;
  }

  if (rpending_.initialized == 0) {
    error.Init(EX_SOFTWARE, "SSLSession::Write(): rpending not initialized");
    return 1;
  }

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::StreamIncomingMsg(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::StreamIncomingMsg(): "
          "Entering: pending msg: %d, %ld, %ld, %ld, %ld.",
          rpending_.storage, rpending_.hdr_len, rpending_.body_len,
          rpending_.buf_offset, rpending_.file_offset);
#endif

  // See how much data we can move over.
  ssize_t n = (rpending_.body_len - rpending_.file_offset) < rbuf_len_ ?
      rpending_.body_len - rpending_.file_offset : rbuf_len_;

  //_LOGGER(LOG_DEBUG, "SSLSession::StreamIncomingMsg(): attempting to move %ld bytes in rbuf_ + %ld (%ld, %ld) to %s (%d, %ld).", n, rpending_.hdr_len, rbuf_len_, rbuf_size_, rfile_.path(NULL).c_str(), rfile_.fd(), rpending_.file_offset);

  // Append all the data we can (or want?) into our file.
  if ((n = write(rfile_.fd(), rbuf_, n)) < 0) {
    error.Init(EX_IOERR, "SSLSession::StreamIncomingMsg(): "
               "write(%s) failed, "
               "n %ld, rbuf len %ld, hdr len %ld: %s",
               rfile_.print().c_str(), n, rbuf_len_, rpending_.hdr_len,
               strerror(errno));
    ResetRbuf();
#if DEBUG_MUTEX_LOCK
    warnx("SSLSession::StreamIncomingMsg(): releasing incoming lock (-1).");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return 1;
  }
   
  rpending_.file_offset += n;

  //_LOGGER(LOG_DEBUG, "SSLSession::StreamIncomingMsg(): removing %ld bytes of data from rbuf_ (%ld, %ld), new offset (%ld).", n, rbuf_len_, rbuf_size_, rpending_.file_offset);

  ShiftRbuf(n, 0);  // remove the buffer data that we just shoved to disk

  // See if we got all of the file.
  if (rpending_.file_offset >= rpending_.body_len) {
    //_LOGGER(LOG_DEBUG, "SSLSession::StreamIncomingMsg(): Closing %ld byte file %s (%ld).", rpending_.body_len, rfile_.path(NULL).c_str(), rpending_.file_offset);

    rfile_.Close();

#if DEBUG_INCOMING_DATA
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::StreamIncomingMsg(): "
            "Leaving: pending msg: %d, %ld, %ld, %ld, %ld.", 
            rpending_.storage, rpending_.hdr_len, rpending_.body_len, 
            rpending_.buf_offset, rpending_.file_offset);
#endif

#if DEBUG_MUTEX_LOCK
    warnx("SSLSession::StreamIncomingMsg(): releasing incoming lock (1).");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return 0;
  }

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::StreamIncomingMsg(): "
          "Leaving: pending msg: %d, %ld, %ld, %ld, %ld.",
          rpending_.storage, rpending_.hdr_len, rpending_.body_len, 
          rpending_.buf_offset, rpending_.file_offset);
#endif

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::StreamIncomingMsg(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
  return 1;  // mark that we're still expecting more data from src
}

// Routine to clean up the buffers and meta-data associated with the
// most-recent incoming message body.  Note, the header is removed
// when initially processed (albeit, it's stored in rhdr_), so the
// only thing in the internal buffer (or file) is the message body.
void SSLSession::ClearIncomingMsg(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::ClearIncomingMsg(): rbuf_ is NULL");
    return;
  }

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::ClearIncomingMsg(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  if (rpending_.storage == SESSION_USE_MEM) {
    //_LOGGER(LOG_DEBUG, "SSLSession::ClearIncomingMsg(): removing %ld byte body from rbuf, new rbuf_len (%ld).", rpending_.body_len, rbuf_len_ -rpending_.body_len);
    ShiftRbuf(rpending_.body_len, 0);
  } else {
    // TODO(aka) Hmm, if this was a tmp file, shold we delete it?
    // Perhaps a flag as the sole parameter to ClearIncomingMsg()?

    rfile_.clear();
  }

  rhdr_.clear();
  memset(&rpending_, 0, sizeof(rpending_));

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::ClearIncomingMsg(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to clean up the buffers and meta-data associated with the
// most-recent outgoing message.
//
// Note, we specifically do *not* clear whdrs_, as this may be used
// for linkability with incoming messages.
void SSLSession::PopOutgoingMsgQueue(void) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::PopOutgoingMsgQueue(): wbuf_ is NULL");
    return;
  }

  if (wpending_.size() == 0) {
    error.Init(EX_SOFTWARE, "SSLSession::PopOutgoingMsgQueue(): "
               "wpending is empty");
    return;
  }

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::PopOutgoingMsgQueue(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::PopOutgoingMsgQueue(): peer %d: "
            "wpending storage (%d), msg_id (%d), "
            "buf_offset (%ld), hdr_len (%ld), body_len (%ld), "
            "file_offset (%ld), size (%d), wbuf (%ld).", 
            handle(), 
            wpending_.front().storage, 
            wpending_.front().msg_id, 
            wpending_.front().buf_offset, 
            wpending_.front().hdr_len, 
            wpending_.front().body_len, 
            wpending_.front().file_offset, 
            wpending_.size(),
            wbuf_len());
  else
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::PopOutgoingMsgQueue(): "
            "peer %d checking: wpending is NULL, wbuf_len = %ld.",
            handle(), wbuf_len());
#endif

  // For convenience, make copies of readonly variables.
  const ssize_t hdr_len = wpending_.front().hdr_len;
  const ssize_t body_len = wpending_.front().body_len;
 
  if (wpending_.front().storage == SESSION_USE_MEM) {
    //_LOGGER(LOG_DEBUG, "SSLSession::PopOutgoingMsgQueue(): removing %ld bytes of header + body from wbuf, new len(%ld), new cnt (%d).", hdr_len + body_len, wbuf_len_ - (hdr_len + body_len), wpending_.size() - 1);

    // Clean up memory buffer (i.e., remove the header & body).
    memmove(wbuf_, wbuf_ + (hdr_len + body_len), 
            wbuf_len_ - (hdr_len + body_len));
    wbuf_len_ -= (hdr_len + body_len);
  } else {
    //_LOGGER(LOG_DEBUG, "SSLSession::PopOutgoingMsgQueue(): removing %ld byte header from wbuf, new len(%ld), and %ld byte file from wfiles, cnt (%d).", hdr_len, wbuf_len_ - hdr_len, body_len, wpending_.size() - 1);
    // Clean up memory buffer (i.e., remove the header) ...
    memmove(wbuf_, wbuf_ + hdr_len, wbuf_len_ - hdr_len);
    wbuf_len_ -= hdr_len;

    //_LOGGER(LOG_DEBUG, "SSLSession::PopOutgoingMsgQueue(): Deleting %ld byte file %s?", wpending_.begin()->body_len, wfiles_.begin()->path(NULL).c_str());
    
    // ... and clean up the file.

    // TODO(aka) Need a flag to signify whether or not the physical
    // file that rfile_ associates with can be deleted when done!

    wfiles_.erase(wfiles_.begin());
  }

  wpending_.erase(wpending_.begin());  // pop wpending_[0]


#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::PopOutgoingMsgQueue(): "
            "Processed outgoing message, next message in peer (%d): "
            "buf_offset (%ld), hdr_len (%ld), "
            "body_len (%ld), file_offset (%ld), size (%d), wbuf (%ld).", 
            handle(),
            wpending_.front().buf_offset, 
            wpending_.front().hdr_len, 
            wpending_.front().body_len, 
            wpending_.front().file_offset, 
            wpending_.size(),
            wbuf_len());
  else
    _LOGGER(LOG_NOTICE, "DEBUG: SSLSession::PopOutgoingMsgQueue(): "
            "Processed outgoing message, "
            "no more outgoing messages in peer (%d).", handle());
#endif

#if DEBUG_MUTEX_LOCK
  warnx("SSLSession::PopOutgoingMsgQueue(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
}

// Boolean functions.

// Routine to check if we have any pending outgoing data sitting in
// this SSLSession.
bool SSLSession::IsOutgoingDataPending(void) const {
  bool data_ready = false;

  // Loop over all outgoing meta-data, looking for data that has not
  // yet been sent.

  for (vector<MsgInfo>::const_iterator msg = wpending_.begin();
       msg != wpending_.end(); msg++) {
    //_LOGGER(LOG_DEBUG, "SSLSession::IsOutgoingDataPending(): Checking to_peer %d wpending: %d, %ld, %ld, %ld, %ld, wbuf len: %ld\n", peer->handle(), msg->storage, msg->hdr_len, msg->body_len, msg->buf_offset, msg->file_offset, peer->wbuf_len());

    if ((msg->storage == SESSION_USE_MEM && (msg->buf_offset < (msg->hdr_len + msg->body_len))) ||
        (msg->storage == SESSION_USE_DISC && ((msg->buf_offset < msg->hdr_len) || 
          (msg->file_offset < msg->body_len)))) {
      data_ready = true;
      break;
    }
  }

  return data_ready;
}

// Private member functions.

// Routine to remove used data from the our internal read buffer.
void SSLSession::ShiftRbuf(const ssize_t len, const ssize_t offset) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::ShiftRbuf(): rbuf_ is NULL");
    return;
  }

  if ((len + offset) > rbuf_len_) {
    _LOGGER(LOG_DEBUG, "SSLSession::ShiftRbuf(): "
            "Len (%ld) + offset (%ld) > rbuf_len (%ld)!",
            len, offset, rbuf_len_);
    ssize_t new_len = rbuf_len_ - offset;  // do what we can
    memmove(rbuf_ + offset, rbuf_ + offset + new_len, 
            rbuf_len_ - (new_len + offset));
    rbuf_len_ -= new_len;
  } else {
    memmove(rbuf_ + offset, rbuf_ + offset + len, rbuf_len_ - (len + offset));
    rbuf_len_ -= len;
  }
}

// Routine to clean up the buffers and meta-data associated with any
// incoming data; used to reset the SSLSession after an error event.
void SSLSession::ResetRbuf(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::ResetRbuf(): rbuf_ is NULL");
    return;
  }

  //ShiftRbuf(rbuf_len_, 0);  // clear *all* data from rbuf_
  rbuf_len_ = 0;

  if (rpending_.storage == SESSION_USE_DISC)
    rfile_.clear();

  rhdr_.clear();
  memset(&rpending_, 0, sizeof(rpending_));
}

// Routine to clean up the buffers and meta-data associated with all
// outgoing messages.
//
// Note, we specifically do *not* clear whdrs_, as this could still be
// used for linkability with incoming messages.
void SSLSession::ResetWbuf(void) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "SSLSession::ResetWbuf(): wbuf_ is NULL");
    return;
  }

  wbuf_len_ = 0;
  wfiles_.clear();
  wpending_.clear();
}

