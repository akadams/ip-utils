/* $Id: MsgHdr.cc,v 1.6 2013/09/13 14:56:38 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <err.h>
#include <stdio.h>
#include <string.h>

#include "Logger.h"
#include "IPComm.h"        // for IPCOMM_PORT_NULL
#include "MsgHdr.h"

#define DEBUG_CLASS 0

// Non-class specific defines & data structures.
uint16_t msg_id_hash = 0;

// Non-class specific utility functions.

// HdrStorage Class.
HdrStorage::HdrStorage(void) 
    : http_() {
#if DEBUG_CLASS
  warnx("HdrStorage::HdrStorage(void) called.");
#endif

  memset(&basic_, 0, sizeof(basic_));
}

HdrStorage::~HdrStorage(void) {
#if DEBUG_CLASS
  warnx("HdrStorage::~HdrStrorage(void) called.");
#endif
}

// Copy constructor, assignment and equality operator, needed for STL.
HdrStorage::HdrStorage(const HdrStorage& src) 
    : http_(src.http_) {
#if DEBUG_CLASS
  warnx("HdrStorage::HdrStorage(const HdrStorage& src) called.");
#endif

  memcpy(&basic_, &src.basic_, sizeof(basic_));
}

HdrStorage& HdrStorage::operator =(const HdrStorage& src) {
#if DEBUG_CLASS
  warnx("HdrStorage::operator =(const HdrStorage& src) called.");
#endif

  memcpy(&basic_, &src.basic_, sizeof(basic_));
  http_ = src.http_;
  
  return *this;
}

// Mutators.
void HdrStorage::set_basic(const struct BasicFramingHdr& basic) {
  memcpy(&basic_, &basic, sizeof(basic_));
}

void HdrStorage::set_http(const HTTPFraming& http) {
  http_ = http;
}

void HdrStorage::clear(void) {
  memset(&basic_, 0, sizeof(basic_));
  http_.clear();
}


// MsgHdr Class.

// Constructors and destructor.
#if 0  // TODO(aka) Void constructor made private (see MsgHdr.h)
MsgHdr::MsgHdr(void) 
  : hdr_() {
#if DEBUG_CLASS
  warnx("MsgHdr::MsgHdr(void) called.");
#endif

  msg_id_ = 0;
  type_ = 0;
}
#endif

MsgHdr::MsgHdr(const uint8_t type) 
  : hdr_() {
#if DEBUG_CLASS
  warnx("MsgHdr::MsgHdr(void) called.");
#endif

  msg_id_ = 0;
  type_ = type;
}

MsgHdr::~MsgHdr(void) {
#if DEBUG_CLASS
  warnx("MsgHdr::~MsgHdr(void) called.");
#endif
}

// Copy constructor, assignment and equality operator, needed for STL.
MsgHdr::MsgHdr(const MsgHdr& src) 
    : hdr_(src.hdr_) {
#if DEBUG_CLASS
  warnx("MsgHdr::MsgHdr(const MsgHdr& src) called.");
#endif

  msg_id_ = src.msg_id_;
  type_ = src.type_;
}

MsgHdr& MsgHdr::operator =(const MsgHdr& src) {
#if DEBUG_CLASS
  warnx("MsgHdr::operator =(const MsgHdr& src) called.");
#endif

  msg_id_ = src.msg_id_;
  type_ = src.type_;
  hdr_ = src.hdr_;

  return *this;
}

// Accessors.
size_t MsgHdr::hdr_len(void) const {
  size_t len = 0;
  switch (type_) {
    case TYPE_BASIC :
      len = kBasicHdrSize;
      break;

    case TYPE_HTTP :
      len = hdr_.http_.hdr_len(false);  // only used on _rhdr in TCPSession
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::hdr_len(): unknown type: %d", type_);
  }

  return len;
}

size_t MsgHdr::body_len(void) const {
  size_t len = 0;
  switch (type_) {
    case TYPE_BASIC :
      len = hdr_.basic_.len;
      break;

    case TYPE_HTTP :
      len = hdr_.http_.msg_len();
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::body_len(): unknown type: %d", type_);
  }

  return len;
}

struct BasicFramingHdr MsgHdr::basic_hdr(void) const { 
  if (type_ != TYPE_BASIC) {
    struct BasicFramingHdr empty;
    memset(&empty, 0, sizeof(empty));
    return empty;
  }

  return hdr_.basic_; 
}

HTTPFraming MsgHdr::http_hdr(void) const { 
  if (type_ != TYPE_HTTP) {
    HTTPFraming empty;
    return empty;
  }

  return hdr_.http_;
}

// Routine to return the *extension type* of the contents of the
// message, e.g., a text/xml media type would return "xml".
string MsgHdr::GetMediaTypeExt(void) {
  string extension(1024, '\0');  // '\0' so strlen works

  switch (type_) {
    case TYPE_BASIC :
      // TODO(aka) Finish implementation!
      _LOGGER(LOG_ERR, "MsgHdr::GetMediaTypeExt(): not implemented yet!");
      return extension;  // return empty string
      break;

    case TYPE_HTTP :
      // Block protect case statement to appease compiler.
      {
        // TOOD(aka) Arguably, this should be done in HTTPFraming!

        // Get copy of vector, cause we're going to use iterators.
        vector<rfc822_msg_hdr> mime_hdrs = hdr_.http_.msg_hdrs();
        vector<rfc822_msg_hdr>::const_iterator mime_hdr = mime_hdrs.begin();
        string media_type;
        while (mime_hdr != mime_hdrs.end()) {
          if (!strncasecmp(mime_hdr->field_name.c_str(),
                           MIME_CONTENT_TYPE, strlen(MIME_CONTENT_TYPE))) {
            media_type = mime_hdr->field_value;
            break;  // found media-type
          }

          mime_hdr++;
        }
        if (!strlen(media_type.c_str())) {
          _LOGGER(LOG_ERR, "MsgHdr::GetMediaTypeExt(): TODO(aka) "
               "Content-Type not set!");
          return extension;  // return empty string
        } 

        // Build filename based on 'Content-Type'.

        // XXX TODO(aka) Add strlen() check prior to strncasecmp()!
        if (!strncasecmp(media_type.c_str(), 
                         MIME_VIDEO_MPEG, strlen(MIME_VIDEO_MPEG))) {
          extension = "mpg";
        } else if (!strncasecmp(media_type.c_str(), 
                         MIME_VIDEO_MP4, strlen(MIME_VIDEO_MP4))) {
          extension = "mp4";
        } else if (!strncasecmp(media_type.c_str(), 
                         MIME_VIDEO_QUICKTIME, strlen(MIME_VIDEO_QUICKTIME))) {
          extension = "mov";
        } else if (!strncasecmp(media_type.c_str(), 
                         MIME_VIDEO_OGG, strlen(MIME_VIDEO_OGG))) {
          extension = "ogg";
        } else if (!strncasecmp(media_type.c_str(), 
                                MIME_APP_TAR, strlen(MIME_APP_TAR))) {
          extension = "tar";
        } else if (!strncasecmp(media_type.c_str(), 
                                MIME_IMAGE_GIF, strlen(MIME_IMAGE_GIF))) {
          extension = "gif";
        } else if (!strncasecmp(media_type.c_str(), 
                                MIME_IMAGE_PNG, strlen(MIME_IMAGE_PNG))) {
          extension = "png";
        } else if (!strncasecmp(media_type.c_str(), 
                                MIME_TEXT_PLAIN, strlen(MIME_TEXT_PLAIN))) {
          extension = "txt";
        } else if (!strncasecmp(media_type.c_str(), 
                                MIME_TEXT_XML, strlen(MIME_TEXT_XML))) {
          extension = "xml";
        } else {
          _LOGGER(LOG_INFO, "MsgHdr::GetMediaTypeExt(): "
                  "Unknown 'Content-Type': %s.", media_type.c_str());
          extension = "dat";
        }
      }
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::GetMediaTypeExt(): unknown type: %d", type_);
      // Fall-through to return what we have so far.
  }

  return extension;
}

// Mutators.
void MsgHdr::set_msg_id(uint16_t msg_id) { 
  msg_id_ = msg_id; 
}

void MsgHdr::set_type(uint8_t type) {
  type_ = type;
}

void MsgHdr::set_hdr(const HdrStorage& hdr) {
  hdr_ = hdr;
}

void MsgHdr::set_body_len(const size_t body_len) {
  switch (type_) {
    case TYPE_BASIC :
      hdr_.basic_.len = body_len;
      break;

    case TYPE_HTTP :
      char tmp_buf[64];
      snprintf((char*)tmp_buf, 64, "%d", (int)body_len);
      hdr_.http_.AppendMsgHdr(MIME_CONTENT_LENGTH, tmp_buf, NULL, NULL);
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::set_body_len(): unknown type: %d", type_);
  }
}

void MsgHdr::clear(void) {
  msg_id_ = 0;
  type_ = TYPE_NONE;
  hdr_.clear();
}

// MsgHdr manipulation.
string MsgHdr::print(void) const {
  string tmp_str(1024, '\0');  // '\0' so strlen works

  switch (type_) {
    case TYPE_BASIC :
      snprintf((char*)tmp_str.c_str(), 1024, 
               "msg_id(%d):hdr.id(%d):hdr.type(%d):hdr.type_id(%d):"
               "hdr.time(%d):hdr.len(%d)",
               msg_id_, hdr_.basic_.id, hdr_.basic_.type, 
               hdr_.basic_.type_id, hdr_.basic_.lamport, hdr_.basic_.len);
      break;

    case TYPE_HTTP :
      // Just output the start line.  (TODO(aka) not sure if the
      // abs_path param is, or will ever be used via MsgHdr.

      snprintf((char*)tmp_str.c_str(), 1024, "%d:%s",
               msg_id_, hdr_.http_.print_start_line(false).c_str());
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::print(): unknown type: %d", type_);
      // Fall-through to return what we have so far.
  }

  return tmp_str;
}

string MsgHdr::print_hdr(const int offset) const {
  string tmp_hdr;

  switch (type_) {
    case TYPE_BASIC :
      // TODO(aka) Finish implementation.
      _LOGGER(LOG_ERR, "MsgHdr::print_hdr(): TYPE_BASIC not implemented yet!");
      tmp_hdr = MsgHdr::print();  // MsgHdr::print() just to do something
      break;

    case TYPE_HTTP :
      tmp_hdr = hdr_.http_.print_hdr(offset, false);  // TODO(aka) see print()
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::print_hdr(): unknown type: %d", type_);
      // Fall-through to return what we have so far.
  }

  return tmp_hdr;
}

// Routine to initialize an HTTPFraming MsgHdr.
//
// This routine can set an ErrorHandler event.
void MsgHdr::Init(const uint16_t msg_id, const HTTPFraming& hdr) { 
  if (type_ != TYPE_HTTP) {
    error.Init(EX_SOFTWARE, "MsgHdr::Init(): type is not HTTP");
    return;
  }

  msg_id_ = msg_id;
  hdr_.http_ = hdr; 
}

// Routine to initialize a BasicFramingHdr MsgHdr.
//
// This routine can set an ErrorHandler event.
void MsgHdr::Init(const uint16_t msg_id, const struct BasicFramingHdr& hdr) {
  if (type_ != TYPE_BASIC) {
    error.Init(EX_SOFTWARE, "MsgHdr::Init(): type is not BASIC");
    return;
  }

  msg_id_ = msg_id;
  memcpy(&hdr_.basic_, &hdr, sizeof(hdr_.basic_)); 
}

// Routine to initialize a MsgHdr object from a char buffer.
//
// This routine can set an ErrorHandler event.
bool MsgHdr::InitFromBuf(const char* buf, const size_t len, size_t* bytes_used,
                         char** chunked_msg_body,
                         size_t* chunked_msg_body_size) {
  switch (type_) {
    case MsgHdr::TYPE_BASIC :
      // Block protect case statement to make compiler happier.
      {
        // We've got ready data, see if we've got enough for the header.
        if (len < sizeof(hdr_.basic_)) {
          _LOGGER(LOG_DEBUG, "MsgHdr::InitFromBuf(): "
                  "note enough data (%d) on %s.", len);
          return false;
        }

        // Grab the message-header and set the msg_id from the header.
        memcpy(&hdr_.basic_, &buf, sizeof(hdr_.basic_));
        msg_id_ = hdr_.basic_.msg_id;
        *bytes_used = sizeof(hdr_.basic_);
      }
      break;

    case MsgHdr::TYPE_HTTP :
      // Block protect case statement to make compiler happier.
      {
        if (!hdr_.http_.InitFromBuf(buf, len, IPCOMM_PORT_NULL,
                                    bytes_used, chunked_msg_body,
                                    chunked_msg_body_size)) {
          if (error.Event())
            error.AppendMsg("MsgHdr::InitFromBuf(): ");
          return false;  // not enough data or ErrorHandler event
        }

        // Note, if the message-body was chunked, it's up to the
        // calling program to *noticed* that chunked_msg_body is
        // non-empty.

        // If we made it here, we parsed the HTTP header, so generate
        // an unique msg_id for the HTTP message.

        msg_id_ = ++msg_id_hash;
      }
      break;

    default :
      error.Init(EX_DATAERR, "MsgHdr::InitFromBuf(): unknown type: %d.", type_);
  }

  return true;
}


// Boolean checks.

bool MsgHdr::IsMsgRequest(void) const {
  bool status = false;
  switch (type_) {
    case TYPE_BASIC :
      // TODO(aka) Finish implementation!
      _LOGGER(LOG_ERR, "MsgHdr::IsMsgStatusNormal(): "
              "TYPE_BASIC not implemented yet!");
      // Fall-through to return what we have so far.
      break;

    case TYPE_HTTP :
      if (hdr_.http_.msg_type() == HTTPFraming::REQUEST)
        status = true;
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::IsMsgStatusNormal(): "
                 "unknown type: %d", type_);
      // Fall-through to return what we have so far.
  }

  return status;
}

// Routine to check the status of a message and return TRUE if the
// message is not an ERROR or CONTROL message.
bool MsgHdr::IsMsgStatusNormal(void) const {
  bool status = false;
  switch (type_) {
    case TYPE_BASIC :
      // TODO(aka) Finish implementation!
      _LOGGER(LOG_ERR, "MsgHdr::IsMsgStatusNormal(): not implemented yet!");
      // Fall-through to return what we have so far.
      break;

    case TYPE_HTTP :
      if (hdr_.http_.status_code() == 200)
        status = true;
      break;

    default :
      _LOGGER(LOG_ERR, "MsgHdr::IsMsgStatusNormal(): unknown type: %d", type_);
      // Fall-through to return what we have so far.
  }

  return status;
}
