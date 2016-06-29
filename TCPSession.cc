/* $Id: TCPSession.cc,v 1.7 2014/04/11 17:42:15 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>        // for lseek(2)

#include "ErrorHandler.h"
#include "Logger.h"

#include "TCPSession.h"

#define DEBUG_CLASS 0
#define DEBUG_INCOMING_DATA 0
#define DEBUG_OUTGOING_DATA 0
#define DEBUG_MUTEX_LOCK 0

// Non-class specific defines & data structures.
const size_t kDefaultBufSize = 4096;
const time_t kDefaultTimeout = 300;  // 5 minutes

uint16_t unique_session_id = 65;  // start at least 6 bits over

// Non-class specific utilities.

// Template Class.

// Constructors and destructor.
TCPSession::TCPSession(const uint8_t framing_type)
    : rfile_(), rhdr_(framing_type), wfiles_(), wpending_(), whdrs_() {
#if DEBUG_CLASS
  warnx("TCPSession::TCPSession(void) called.");
#endif

  framing_type_ = framing_type;
  handle_ = unique_session_id++;
  timeout_ = time(NULL) + kDefaultTimeout;
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

TCPSession::~TCPSession(void) {
#if DEBUG_CLASS
  warnx("TCPSession::~TCPSession(void) called.");
#endif

  if (rbuf_)
    free((void*)rbuf_);
  if (wbuf_)
    free((void*)wbuf_);

  pthread_mutex_destroy(&incoming_mtx);
  pthread_mutex_destroy(&outgoing_mtx);
}

// Copy constructor, assignment and equality operator needed for STL.
TCPSession::TCPSession(const TCPSession& src)
    : SSLConn(src), rfile_(src.rfile_), rhdr_(src.rhdr_), 
      wfiles_(src.wfiles_), wpending_(src.wpending_), whdrs_(src.whdrs_) {
#if DEBUG_CLASS
  warnx("TCPSession::TCPSession(const TCPSession&) called.");
#endif

  rbuf_ = NULL;
  rbuf_size_ = 0;
  rbuf_len_ = 0;
  wbuf_ = NULL;
  wbuf_size_ = 0;
  wbuf_len_ = 0;

  // Note, usaully one must call TCPSession::Init() to generate
  // buffers (as we allow Init() to set ErrorHandler events, however,
  // we don't have that luxury here.  Hence, we must handle errors in
  // here, which may prove to be difficult.  Damn my desire to put
  // networking objects in the STL!

  if (src.rbuf_size_ > 0) {
    if ((rbuf_ = (char*)malloc(src.rbuf_size_)) == NULL) {
      _LOGGER(LOG_ERR, "TCPSession(const TCPSession& src): malloc(%ld) failed",
              src.rbuf_size_);
      return;
    }
  }
  if (src.wbuf_size_ > 0) {
    if ((wbuf_ = (char*)malloc(src.wbuf_size_)) == NULL) {
      _LOGGER(LOG_ERR, "TCPSession(const TCPSession& src): malloc(%ld) failed",
              src.wbuf_size_);
      return;
    } 
  }

  // If we made it here, our mallocs worked, so set the rest of the object.
  framing_type_ = src.framing_type_;
  handle_ = src.handle_;
  timeout_ = src.timeout_;
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
TCPSession::operator =(const TCPSession& src) {
#if DEBUG_CLASS
  warnx("TCPSession::operator =(const TCPSession&) called.");
#endif

}
#endif

#if 0  // XXX
int TCPSession::operator ==(const TCPSession& other) const {
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
void TCPSession::set_handle(const uint16_t handle) { 
  handle_ = handle; 
}

void TCPSession::set_synchronize_status(const uint8_t synchronize_status) {
  synchronize_status_ = synchronize_status;
}

// Wrapper function for TCPConn::set_connected().  This is needed,
// because prior to calling TCPConn::set_connected(), we need to make
// sure we have both locks (which TCPConn knows nothing about).
void TCPSession::set_connected(const bool connected) {
#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_connected(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  // Note, multiple locking order is: incoming, than outgoing!
#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_connected(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  TCPConn::set_connected(connected);

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_connected(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_connected(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to setup the streaming *read* File object.
//
// Note, this routine can set an ErrorHandler event.
void TCPSession::set_rfile(const char* path, const ssize_t len) {
  if (path == NULL || strlen(path) == 0) {
    error.Init(EX_SOFTWARE, "TCPSession::set_rfile(): path is NULL or empty");
    return;
  }

  if (rfile_.IsOpen()) {
    error.Init(EX_SOFTWARE, "TCPSession::set_rfile(): rfile is open");
    return;
  }

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_rfile(): requesting incoming lock.");
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
    error.AppendMsg("TCPSession::set_rfile(): ");

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_rfile(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to set the Incoming Message thead id.
void TCPSession::set_rtid(const pthread_t rtid) {
#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_rtid(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  rtid_ = rtid;

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_rtid(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

void TCPSession::set_storage(const int storage) {
#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_storage(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  rpending_.storage = storage;
  rpending_.storage_initialized = true;

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::set_storage(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to *erase* a specific MsgHdr from our list (whdrs_).
void TCPSession::delete_whdr(const uint16_t msg_id) {
#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::delete_whdr(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  // Loop through our list and find the culprit ...
  list<MsgHdr>::iterator itr = whdrs_.begin();
  while (itr != whdrs_.end()) {
    if (itr->msg_id() == msg_id)
      break;
  
    itr++;
  }
  if (itr != whdrs_.end())
    whdrs_.erase(itr);
  else
    _LOGGER(LOG_INFO, "TCPSession::delete_whdr(): Unable to find msg-id: %d",
            msg_id);

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::delete_whdr(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
}


// TCPSession manipulation.

// Routine to *pretty* print object.
string TCPSession::print(void) const {
  // Print retuns the host, port and socket.
  string tmp_str(kDefaultBufSize, '\0');

  snprintf((char*)tmp_str.c_str(), kDefaultBufSize, 
           "%s:%d:%lu:%ld:%ld:%d:%s:%ld:%ld:%ld:%ld:%ld", 
           SSLConn::print().c_str(), handle_, rtid_,
           (long)rbuf_size_, (long)rbuf_len_, (int)rpending_.initialized,
           rfile_.print().c_str(), (long)wbuf_size_, (long)wbuf_len_,
           (long)wfiles_.size(), (long)wpending_.size(), (long)whdrs_.size());


  return tmp_str;
}

void TCPSession::Init(void) {
  if (rbuf_ != NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::Init(): rbuf is not NULL");
    return;
  }

  if (rtid_ != TCPSESSION_THREAD_NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::Init(): rtid is not NULL");
    return;
  }

  if ((rbuf_ = (char*)calloc(kDefaultBufSize, 1)) == NULL) {
    error.Init(EX_OSERR, "TCPSession::Init(): rbuf calloc(%d) failed", 
               kDefaultBufSize);
    return;
  }

  rbuf_size_ = kDefaultBufSize;
  rbuf_len_ = 0;

  if ((wbuf_ = (char*)malloc(kDefaultBufSize)) == NULL) {
    error.Init(EX_OSERR, "TCPSession::Init(): wbuf malloc(%d) failed", 
               kDefaultBufSize);
    return;
  }

  wbuf_size_ = kDefaultBufSize;
  wbuf_len_ = 0;
}

// Routine to setup or initialize the incoming message on a peer.  If
// we have aneough data, i.e., InitFromBuf() returns TRUE, we
// initialize our incoming message's meta-data (rpending_).
bool TCPSession::InitIncomingMsg(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::InitIncomingMsg(): rbuf is NULL");
    return false;
  }

  if (rbuf_len_ == 0)
    return false;  // nothing to do

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::InitIncomingMsg(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  // Set aside space in case our incoming message-body is chunked.
  char* chunked_msg_body = NULL;
  size_t chunked_msg_body_size = kDefaultBufSize;
  if ((chunked_msg_body = (char*)calloc(chunked_msg_body_size, 1)) == NULL) {
    error.Init(EX_OSERR, "TCPSession::InitIncomingMsg(): "
               "calloc(3) failed for size %ulb", chunked_msg_body_size);
    return false;
  }

  size_t bytes_used = 0;  // amount of data used from rbuf_ to build header
  if (!rhdr_.InitFromBuf(rbuf_, rbuf_len_, &bytes_used,
                         &chunked_msg_body, &chunked_msg_body_size)) {
    if (error.Event()) {
      error.AppendMsg("TCPSession::InitIncomingMsg(): ");
      ResetRbuf();
    }

    if (chunked_msg_body != NULL)
      free(chunked_msg_body);

#if DEBUG_MUTEX_LOCK
    warnx("TCPSession::InitIncomingMsg(): releasing incoming lock.");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return false;  // not enough data or ErrorHandler event
  }

  // If we made it here, we parsed the framing header, so remove the
  // framing header from our buffer.  First, check the null-terminated
  // chunked_msg_body to see if we were forced to slurp up the
  // message-body during our header parse.

  size_t chunked_msg_body_len = strlen(chunked_msg_body);
  if (chunked_msg_body_len > 0) {
    // Copy our chunked messsage-body back into our rbuf_.  Yes, I
    // know this is an additional copy (and arguably a HACK), but the
    // rest of the TCPSession library wants to operate on rbuf_ using
    // body_len in MsgHdr (MsgInfo).
  
    _LOGGER(LOG_INFO, "TCPSession::InitIncomingMsg(): "
            "Moving chunked data back to rbuf_: "
            "chunked msg-body (%ld), bytes_used (%ld), rbuf_len (%ld).",
            chunked_msg_body_len, bytes_used, rbuf_len_);

    memcpy(rbuf_, chunked_msg_body, chunked_msg_body_len);

    // We copied the msg-body back in, now close the gap between the
    // end of the msg-body and the next message waiting in rbuf_ (if
    // one exists).

    ShiftRbuf((bytes_used - chunked_msg_body_len), chunked_msg_body_len);

    // Finally, add a Content-Length value to our message-headers
    // (this will allow the rest of the routines, e.g.,
    // MsgHdr::body_len(), to behave as if the message wasn't chunked.

    rhdr_.set_body_len(chunked_msg_body_len);
  } else {
    ShiftRbuf(bytes_used, 0);
  }

  if (chunked_msg_body != NULL)
    free(chunked_msg_body);

  // Build our TCPSession meta-data for the incoming message.
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
  warnx("TCPSession::InitIncomingMsg(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
  return true;
}

// Routine to install a new message (stored in memory) into our
// outgoing message queue.
//
// This routine can set an ErrorHandler event.
bool TCPSession::AddMsgBuf(const char* framing_hdr, const ssize_t hdr_len, 
                           const char* msg_body, const ssize_t body_len,
                           const MsgHdr& whdr) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::AddMsgBuf(): wbuf_ is NULL");
    return false;
  }

  if (framing_hdr == NULL || strlen(framing_hdr) == 0) {
    error.Init(EX_SOFTWARE, "TCPSession::AddMsgBuf(): "
               "framing_hdr is NULL or empty");
    return false;
  }

  // TODO(aka) The problem with this routine is that the four
  // components that make up a message (framing_hdr & msg_body in
  // wbuf_, whdr_ and wpending_) are *not* linked.  That is, we assume
  // that the next hdr & body in wbuf_ will match the next MsgInfo and
  // MsgHdr in wpending_ and whdrs_ respectively.  But I'm not sure
  // that's a safe assumption!

  // TODO(aka) Furthermore, this routine should be able to handle a NULL msg_body!

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::AddMsgBuf(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  // Make sure we have enough room for the msg header & body
  if ((hdr_len + body_len + wbuf_len_) > wbuf_size_) {
    _LOGGER(LOG_DEBUG, "TCPSession::AddMsgBuf(): reallocing wbuf_, msg_len (%ld) + current wlen (%ld) is greater than wbuf_size (%ld).", hdr_len + body_len, wbuf_len_, wbuf_size_);

    ssize_t new_wbuf_size = hdr_len + body_len + wbuf_len_ + kDefaultBufSize;
    char* p = (char*)realloc(wbuf_, new_wbuf_size);
    if (p == NULL) {
      // Set an ErrorHandler event and return.
      error.Init(EX_OSERR, "TCPSession::AddMsgBuf(): realloc(%ld) failed", 
                 new_wbuf_size);
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::AddMsgBuf(): releasing outgoing lock.");
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
  warnx("TCPSession::AddMsgBuf(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
  return true;
}

// Routine to install a new message (stored in memory) into our
// outgoing message queue.
bool TCPSession::AddMsgFile(const char* framing_hdr, const ssize_t hdr_len, 
                            const File& msg_body, const ssize_t body_len,
                            const MsgHdr& whdr) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::AddMsgFile(): wbuf is NULL");
    return false;
  }

  if (framing_hdr == NULL || strlen(framing_hdr) == 0) {
    error.Init(EX_SOFTWARE, "TCPSession::AddMsgFile(): "
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
  warnx("TCPSession::AddMsgFile(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

  // Make sure we have enough room for the msg header & body.
  if (hdr_len + wbuf_len_ > wbuf_size_) {
    _LOGGER(LOG_DEBUG, "TCPSession::AddMsgFile(): reallocing wbuf_, hdr_len (%ld) + current wlen (%ld) is greater than wbuf_size (%ld).", hdr_len, wbuf_len_, wbuf_size_);

    ssize_t new_wbuf_size = hdr_len + wbuf_len_ + kDefaultBufSize;
    char* p = (char*)realloc(wbuf_, new_wbuf_size);
    if (p == NULL) {
      // Set an ErrorHandler event and return.
      error.Init(EX_OSERR, "TCPSession::AddMsgFile(): "
                 "realloc(%ld) failed", new_wbuf_size);
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::AddMsgFile(): releasing outgoing lock (-1).");
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
  warnx("TCPSession::AddMsgFile(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
  return true;
}

// Routine to read, via SSLConn::Read(), any data on the socket and
// store in our internal buffer (rbuf_).
//
// Note, this routine can set an ErrorHandler event.
ssize_t TCPSession::Read(bool* eof) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::Read(): rbuf is NULL");
    return 0;
  }

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::Read(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  //_LOGGER(LOG_DEBUG, "TCPSession::Read(): calling Read(), session: %s.", print().c_str());

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::Read(): "
          "Entering: rbuf: %ld, %ld, eof: %d, "
          "rpending: %d, %d, %ld, %ld, %ld, %ld.", 
          rbuf_size_, rbuf_len_, *eof,
          rpending_.storage, rpending_.storage_initialized, 
          rpending_.hdr_len, rpending_.body_len, rpending_.buf_offset,
          rpending_.file_offset);
#endif

  // Call SSLConn::Read() to get the work done.
  ssize_t bytes_read = 
      SSLConn::Read(rbuf_size_ - rbuf_len_, rbuf_ + rbuf_len_, eof);
  if (error.Event()) {
    error.AppendMsg("TCPSession::Read(): "
                    "rbuf_ %p, rbuf_len_ %ld, rbuf_size_ %ld, eof %d: "
                    "clearing rbuf_: ", 
                    rbuf_, rbuf_len_, rbuf_size_, *eof);
    ResetRbuf();
#if DEBUG_MUTEX_LOCK
    warnx("TCPSession::Read(): releasing incoming lock (-1).");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return 0;
  }

  rbuf_len_ += bytes_read;

  // Resize rbuf_ if we're out of room.
  if (rbuf_len_ == rbuf_size_) {
    _LOGGER(LOG_DEBUG, "TCPSession::Read(): reallocing rbuf_"
            ", rbuf_len %ld, rbuf_size %ld.",
            rbuf_len_, rbuf_size_);
    ssize_t new_rbuf_size = rbuf_size_ + kDefaultBufSize;
    char* p = (char*)realloc(rbuf_, new_rbuf_size);
    if (p == NULL) {
      // Set an ErrorHandler event and return.
      error.Init(EX_OSERR, "TCPSession::Read(): realloc(%ld) failed", 
                 new_rbuf_size);
      ResetRbuf();
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::Read(): releasing incoming lock (-2).");
#endif
      pthread_mutex_unlock(&incoming_mtx);
      return 0;
    } else {
      rbuf_ = p;
      rbuf_size_ = new_rbuf_size;
    }
  }

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::Read(): "
          "Leaving: rbuf: %ld, %ld, eof: %d, "
          "rpending: %d, %ld, %ld, %ld, %ld.", 
          rbuf_size_, rbuf_len_, *eof, 
          rpending_.storage, rpending_.hdr_len, rpending_.body_len, 
          rpending_.buf_offset, rpending_.file_offset);
#endif

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::Read(): releasing incoming lock.");
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
ssize_t TCPSession::Write(void) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::Write(): wbuf is NULL");
    return 0;
  }

  if (wpending_.size() == 0) {
    error.Init(EX_SOFTWARE, "TCPSession::Write(): wpending is empty");
    return 0;
  }

  if (!IsConnected()) {
    error.Init(EX_SOFTWARE, "TCPSession::Write(): not connected");
    return 0;
  }

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::Write(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::Write(): "
            "Enter: pending msg: %d, %ld, %ld, %ld, %ld.", 
            wpending_.front().storage, wpending_.front().hdr_len, 
            wpending_.front().body_len, wpending_.front().buf_offset, 
            wpending_.front().file_offset);
  else
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::Write(): "
            "Enter: pending msg NULL.");
#endif

  // For convenience, make copies of readonly variables.
  const ssize_t hdr_len = wpending_.front().hdr_len;
  const ssize_t body_len = wpending_.front().body_len;

  ssize_t bytes_sent = 0;

  if (wpending_.front().storage == SESSION_USE_MEM) {
    // Message in within the internal memory buffer (wbuf_).

    //_LOGGER(LOG_DEBUG, "TCPSession::Write(): lengths_ is of size %d.", lengths_.size());

    ssize_t msg_len = hdr_len + body_len;  // message is all within wbuf_

    // Send the message out (header & body) using SSLConn::Write().
    bytes_sent = 
        SSLConn::Write(wbuf_ + wpending_.front().buf_offset, 
                       msg_len - wpending_.front().buf_offset);
    if (error.Event()) {
      error.AppendMsg("TCPSession::Write(): "
                      "wbuf_ %p, wbuf_len_ %ld, wbuf_size_ %ld"
                      ", msg_len %ld, offset %ld: ", 
                      wbuf_, wbuf_len_, wbuf_size_, msg_len,
                      wpending_.front().buf_offset);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::Write(): releasing outgoing lock (1).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return 0;
    }

    wpending_.front().buf_offset += bytes_sent;  // update offset in queue

    if ((bytes_sent + wpending_.front().buf_offset) >= msg_len) {
      // Yeah, we sent all the data!

      //_LOGGER(LOG_DEBUG, "TCPSession::Write(): Should I remove %ld byte message from wbuf, new len(%ld), cnt (%d).", msg_len, wbuf_len_, wpending_.size());
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
        error.AppendMsg("TCPSession::Write(): "
                        "wbuf_ %p, wbuf_len_ %ld, wbuf_size_ %ld"
                        ", hdr_len %ld, offset %ld: ", 
                        wbuf_, wbuf_len_, wbuf_size_, hdr_len,
                        wpending_.front().buf_offset);
        ResetWbuf();
#if DEBUG_MUTEX_LOCK
        warnx("TCPSession::Write(): releasing outgoing lock (2).");
#endif
        pthread_mutex_unlock(&outgoing_mtx);
        return 0;
      }
      
      wpending_.front().buf_offset += bytes_sent;  // update offset in queue
    }

    if (wpending_.front().buf_offset < hdr_len) {
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::Write(): releasing outgoing lock (3).");
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
        error.AppendMsg("TCPSession::Write(): current offset %ld: ", 
                        wpending_.front().file_offset);
        ResetWbuf();
#if DEBUG_MUTEX_LOCK
        warnx("TCPSession::Write(): releasing outgoing lock (4).");
#endif
        pthread_mutex_unlock(&outgoing_mtx);
        return 0;
      }
      
      // Since we needed to open it, let's jump to the offset.
      if (lseek(wfiles_.front().fd(), wpending_.front().file_offset, 
                SEEK_SET) < 0) {
        error.Init(EX_IOERR, "TCPSession::Write(): lseek(%s) failed: %s",
                   wfiles_.front().print().c_str(), strerror(errno));
        ResetWbuf();
#if DEBUG_MUTEX_LOCK
        warnx("TCPSession::Write(): releasing outgoing lock (5).");
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
      error.Init(EX_IOERR, "TCPSession::Write(): TOOD(aka) "
                 "read EOF from %s, but file_offset is %ld, size is %ld",
                 wfiles_.front().print().c_str(), 
                 wpending_.front().file_offset, body_len);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::Write(): releasing outgoing lock (6).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return bytes_sent;
    } else if (n < 0) {
      // ERROR
      error.Init(EX_IOERR, "TCPSession::Write(): read(%s) failed"
                 ", file_offset is %ld, size is %ld",
                 wfiles_.front().print().c_str(),
                 wpending_.front().file_offset, body_len);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::Write(): releasing outgoing lock (7).");
#endif
      pthread_mutex_unlock(&outgoing_mtx);
      return bytes_sent;
    }

    // ... and send the next chunk out using SSLConn::Write()
    bytes_sent = SSLConn::Write(tmp_buf, n);  // redo bytes_sent
    if (error.Event()) {
      error.AppendMsg("TCPSession::Write(): "
                      "file %s, body_len %ld, file_offset %ld: ", 
                      wfiles_.front().print().c_str(), body_len,
                      wpending_.front().file_offset);
      ResetWbuf();
#if DEBUG_MUTEX_LOCK
      warnx("TCPSession::Write(): releasing outgoing lock (8).");
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
      printf("TCPSession::Write(): DEBUG: memmoving %ld bytes at %p to %p.\n", wbuf_len_ - msg_len, wbuf_ + msg_len, wbuf_);
      memmove(wbuf_, wbuf_ + msg_len, wbuf_len_ - msg_len);
      wbuf_len_ -= msg_len;
      wpending_.pop();

      _LOGGER(LOG_DEBUG, "TCPSession::Write(): removed %ld byte message from wbuf, new len(%ld), cnt (%d).", msg_len, wbuf_len_, wpending_.size());
      */
      //_LOGGER(LOG_DEBUG, "TCPSession::Write(): Should I remove %ld byte message from wfiles, new len(%ld), cnt (%d).", body_len, wbuf_len_, wpending_.size());
    }
  }  // else if (wpending_.front().storage == SESSION_USE_MEM) {

#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::Write(): "
            "Leaving: pending msg: %d, %ld, %ld, %ld, %ld.",
            wpending_.front().storage, wpending_.front().hdr_len,
            wpending_.front().body_len, wpending_.front().buf_offset, 
            wpending_.front().file_offset);
  else
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::Write(): "
            "Leaving: pending msg NULL.");
#endif

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::Write(): releasing outgoing lock.");
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
int TCPSession::StreamIncomingMsg(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::StreamIncomingMsg(): rbuf is NULL");
    return 1;
  }

  if (!rfile_.IsOpen()) {
    error.Init(EX_SOFTWARE, "TCPSession::StreamIncomingMsg(): rfile not open");
    return 1;
  }

  if (rpending_.initialized == 0) {
    error.Init(EX_SOFTWARE, "TCPSession::Write(): rpending not initialized");
    return 1;
  }

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::StreamIncomingMsg(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::StreamIncomingMsg(): "
          "Entering: pending msg: %d, %ld, %ld, %ld, %ld.",
          rpending_.storage, rpending_.hdr_len, rpending_.body_len,
          rpending_.buf_offset, rpending_.file_offset);
#endif

  // See how much data we can move over.
  ssize_t n = (rpending_.body_len - rpending_.file_offset) < rbuf_len_ ?
      rpending_.body_len - rpending_.file_offset : rbuf_len_;

  //_LOGGER(LOG_DEBUG, "TCPSession::StreamIncomingMsg(): attempting to move %ld bytes in rbuf_ + %ld (%ld, %ld) to %s (%d, %ld).", n, rpending_.hdr_len, rbuf_len_, rbuf_size_, rfile_.path(NULL).c_str(), rfile_.fd(), rpending_.file_offset);

  // Append all the data we can (or want?) into our file.
  if ((n = write(rfile_.fd(), rbuf_, n)) < 0) {
    error.Init(EX_IOERR, "TCPSession::StreamIncomingMsg(): "
               "write(%s) failed, "
               "n %ld, rbuf len %ld, hdr len %ld: %s",
               rfile_.print().c_str(), n, rbuf_len_, rpending_.hdr_len,
               strerror(errno));
    ResetRbuf();
#if DEBUG_MUTEX_LOCK
    warnx("TCPSession::StreamIncomingMsg(): releasing incoming lock (-1).");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return 1;
  }
   
  rpending_.file_offset += n;

  //_LOGGER(LOG_DEBUG, "TCPSession::StreamIncomingMsg(): removing %ld bytes of data from rbuf_ (%ld, %ld), new offset (%ld).", n, rbuf_len_, rbuf_size_, rpending_.file_offset);

  ShiftRbuf(n, 0);  // remove the buffer data that we just shoved to disk

  // See if we got all of the file.
  if (rpending_.file_offset >= rpending_.body_len) {
    //_LOGGER(LOG_DEBUG, "TCPSession::StreamIncomingMsg(): Closing %ld byte file %s (%ld).", rpending_.body_len, rfile_.path(NULL).c_str(), rpending_.file_offset);

    rfile_.Close();

#if DEBUG_INCOMING_DATA
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::StreamIncomingMsg(): "
            "Leaving: pending msg: %d, %ld, %ld, %ld, %ld.", 
            rpending_.storage, rpending_.hdr_len, rpending_.body_len, 
            rpending_.buf_offset, rpending_.file_offset);
#endif

#if DEBUG_MUTEX_LOCK
    warnx("TCPSession::StreamIncomingMsg(): releasing incoming lock (1).");
#endif
    pthread_mutex_unlock(&incoming_mtx);
    return 0;
  }

#if DEBUG_INCOMING_DATA
  _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::StreamIncomingMsg(): "
          "Leaving: pending msg: %d, %ld, %ld, %ld, %ld.",
          rpending_.storage, rpending_.hdr_len, rpending_.body_len, 
          rpending_.buf_offset, rpending_.file_offset);
#endif

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::StreamIncomingMsg(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
  return 1;  // mark that we're still expecting more data from src
}

// Routine to clean up the buffers and meta-data associated with the
// most-recent incoming message body.  Note, the header is removed
// when initially processed (albeit, it's stored in rhdr_), so the
// only thing in the internal buffer (or file) is the message body.
void TCPSession::ClearIncomingMsg(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::ClearIncomingMsg(): rbuf_ is NULL");
    return;
  }

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::ClearIncomingMsg(): requesting incoming lock.");
#endif
  pthread_mutex_lock(&incoming_mtx);

  if (rpending_.storage == SESSION_USE_MEM) {
    //_LOGGER(LOG_DEBUG, "TCPSession::ClearIncomingMsg(): removing %ld byte body from rbuf, new rbuf_len (%ld).", rpending_.body_len, rbuf_len_ -rpending_.body_len);
    ShiftRbuf(rpending_.body_len, 0);
  } else {
    // TODO(aka) Hmm, if this was a tmp file, shold we delete it?
    // Perhaps a flag as the sole parameter to ClearIncomingMsg()?

    rfile_.clear();
  }

  rhdr_.clear();
  memset(&rpending_, 0, sizeof(rpending_));

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::ClearIncomingMsg(): releasing incoming lock.");
#endif
  pthread_mutex_unlock(&incoming_mtx);
}

// Routine to clean up the buffers and meta-data associated with the
// most-recent outgoing message.
//
// Note, we specifically do *not* clear whdrs_, as this may be used
// for linkability with incoming messages.
void TCPSession::PopOutgoingMsgQueue(void) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::PopOutgoingMsgQueue(): wbuf_ is NULL");
    return;
  }

  if (wpending_.size() == 0) {
    error.Init(EX_SOFTWARE, "TCPSession::PopOutgoingMsgQueue(): "
               "wpending is empty");
    return;
  }

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::PopOutgoingMsgQueue(): requesting outgoing lock.");
#endif
  pthread_mutex_lock(&outgoing_mtx);

#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::PopOutgoingMsgQueue(): peer %d: "
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
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::PopOutgoingMsgQueue(): "
            "peer %d checking: wpending is NULL, wbuf_len = %ld.",
            handle(), wbuf_len());
#endif

  // For convenience, make copies of readonly variables.
  const ssize_t hdr_len = wpending_.front().hdr_len;
  const ssize_t body_len = wpending_.front().body_len;
 
  if (wpending_.front().storage == SESSION_USE_MEM) {
    //_LOGGER(LOG_DEBUG, "TCPSession::PopOutgoingMsgQueue(): removing %ld bytes of header + body from wbuf, new len(%ld), new cnt (%d).", hdr_len + body_len, wbuf_len_ - (hdr_len + body_len), wpending_.size() - 1);

    // Clean up memory buffer (i.e., remove the header & body).
    memmove(wbuf_, wbuf_ + (hdr_len + body_len), 
            wbuf_len_ - (hdr_len + body_len));
    wbuf_len_ -= (hdr_len + body_len);
  } else {
    //_LOGGER(LOG_DEBUG, "TCPSession::PopOutgoingMsgQueue(): removing %ld byte header from wbuf, new len(%ld), and %ld byte file from wfiles, cnt (%d).", hdr_len, wbuf_len_ - hdr_len, body_len, wpending_.size() - 1);
    // Clean up memory buffer (i.e., remove the header) ...
    memmove(wbuf_, wbuf_ + hdr_len, wbuf_len_ - hdr_len);
    wbuf_len_ -= hdr_len;

    //_LOGGER(LOG_DEBUG, "TCPSession::PopOutgoingMsgQueue(): Deleting %ld byte file %s?", wpending_.begin()->body_len, wfiles_.begin()->path(NULL).c_str());
    
    // ... and clean up the file.

    // TODO(aka) Need a flag to signify whether or not the physical
    // file that rfile_ associates with can be deleted when done!

    wfiles_.erase(wfiles_.begin());
  }

  wpending_.erase(wpending_.begin());  // pop wpending_[0]


#if DEBUG_OUTGOING_DATA
  if (wpending_.size())
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::PopOutgoingMsgQueue(): "
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
    _LOGGER(LOG_NOTICE, "DEBUG: TCPSession::PopOutgoingMsgQueue(): "
            "Processed outgoing message, "
            "no more outgoing messages in peer (%d).", handle());
#endif

#if DEBUG_MUTEX_LOCK
  warnx("TCPSession::PopOutgoingMsgQueue(): releasing outgoing lock.");
#endif
  pthread_mutex_unlock(&outgoing_mtx);
}

// Boolean functions.

#if 0  // Decprecated.
// Routine to make sure we have all of our message (hdr + body).
bool TCPSession::IsIncomingMsgComplete(void) const {
  if (rpending_.initialized == 0)
    return false;  // need to call InitIncomingMsg()

  if (framing_type_ == MsgHdr::TYPE_BASIC)
    return ((rpending_.file_offset >= rpending_.body_len) ||
             (rbuf_len_ >= rpending_.body_len)) ? true : false;
}
#endif

// Routine to check if we have any pending outgoing data sitting in
// this TCPSession.
bool TCPSession::IsOutgoingDataPending(void) const {
  bool data_ready = false;

  // Loop over all outgoing meta-data, looking for data that has not
  // yet been sent.

  for (vector<MsgInfo>::const_iterator msg = wpending_.begin();
       msg != wpending_.end(); msg++) {
    //_LOGGER(LOG_DEBUG, "TCPSession::IsOutgoingDataPending(): Checking to_peer %d wpending: %d, %ld, %ld, %ld, %ld, wbuf len: %ld\n", peer->handle(), msg->storage, msg->hdr_len, msg->body_len, msg->buf_offset, msg->file_offset, peer->wbuf_len());

    if ((msg->storage == SESSION_USE_MEM &&
         (msg->buf_offset < (msg->hdr_len + msg->body_len))) ||
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
void TCPSession::ShiftRbuf(const ssize_t len, const ssize_t offset) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::ShiftRbuf(): rbuf_ is NULL");
    return;
  }

  if ((len + offset) > rbuf_len_) {
    _LOGGER(LOG_DEBUG, "TCPSession::ShiftRbuf(): "
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
// incoming data; used to reset the TCPSession after an error event.
void TCPSession::ResetRbuf(void) {
  if (rbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::ResetRbuf(): rbuf_ is NULL");
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
void TCPSession::ResetWbuf(void) {
  if (wbuf_ == NULL) {
    error.Init(EX_SOFTWARE, "TCPSession::ResetWbuf(): wbuf_ is NULL");
    return;
  }

  wbuf_len_ = 0;
  wfiles_.clear();
  wpending_.clear();
}

