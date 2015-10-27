/* $Id: HTTPFraming.cc,v 1.5 2013/09/13 14:56:38 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <err.h>
#include <inttypes.h>
#include <stdlib.h>

#include <string>
using namespace std;

#include "ErrorHandler.h"
#include "Logger.h"

#include "HTTPFraming.h"

#define DEBUG_CLASS 0
#define DEBUG_PARSE 0

// Non-class specific defines & data structures.
#define SCRATCH_BUF_SIZE (1024 * 4)  // big enough to hold HTTP header

// Schemes.
static const char* kHTTPSlash = "HTTP/";
/*  TODO(aka) Deprecated
static const char* kVersion0_9 = "HTTP/0.9";
static const char* kVersion1_0 = "HTTP/1.0";
static const char* kVersion1_1 = "HTTP/1.1";
*/

// Methods.
static const char* kMethodNone = "NONE";
static const char* kMethodGet = "GET";
static const char* kMethodHead = "HEAD";
static const char* kMethodPost = "POST";
static const char* kMethodPut = "PUT";
static const char* kMethodDelete = "DELETE";
static const char* kMethodTrace = "TRACE";
static const char* kMethodConnect = "CONNECT";
static const char* kMethodOptions = "OPTIONS";
static const int kNumMethods = 9;

// HTTP "start-line" types.
static const char* kTypeNotReady = "NOT READY";
static const char* kTypeRequest = "REQUEST";
static const char* kTypeResponse = "RESPONSE";

// Message-header field types & values.
static const char* kValueClose = "close";

static const char* code_200_phrases[] = {
  "OK",
  "Created",
  "Accepted",
  "Non-Authoritative Information",
  "No Content",
};

static const char* code_400_phrases[] = {
  "Bad Request",
  "Unauthorized",
  "Payment Required",
  "Forbidden",
  "Method Not Allowed",
};

static const char* code_500_phrases[] = {
  "Internal Server Error",
  "Not Implemented",
  "Bad Gateway",
  "Service Unavailable",
  "Gateway Time-out",
  "HTTP Version not supported",
};

static const char* method_names[kNumMethods] = {
  kMethodNone,
  kMethodGet,
  kMethodHead,
  kMethodPost,
  kMethodPut,
  kMethodDelete,
  kMethodTrace,
  kMethodConnect,
  kMethodOptions,
};

static const char* type_names[3] = {
  kTypeNotReady,
  kTypeRequest,
  kTypeResponse,
};

// Non-class specific utility functions.
const char* status_code_phrase(const int status_code) {
  if (status_code >= 200 && status_code <= 204)
    return code_200_phrases[status_code - 200];
  else if (status_code >= 400 && status_code <= 404)
    return code_400_phrases[status_code - 400];
  else if (status_code >= 500 && status_code <= 505)
    return code_500_phrases[status_code - 500];
  else 
    return "NULL Status Phrase";
}

const char* method_name(const int method) {
  if (method < 0 || method > kNumMethods)
    return method_names[0];

  return method_names[method];
}

const int method_id(const char* method_name) {
  for (int i = 0; i < kNumMethods; i++) {
    if (! strncasecmp(method_names[i], method_name, 
                      kNumMethods))
      return i;
  }

  return -1;
}

const char* start_line_name(const int type) {
  if (type < 0 || type > 3)
    return type_names[0];

  return type_names[type];
}

// HTTPFraming Class.

// Constructors and destructor.
HTTPFraming::HTTPFraming(void)
    : uri_(), msg_hdrs_() {
#if DEBUG_CLASS
  warnx("HTTPFraming::HTTPFraming(void) called.");
#endif

  major_ = HTTPFRAMING_VERSION_MAJOR;
  minor_ = HTTPFRAMING_VERSION_MINOR;
  msg_type_ = NOT_READY;
  method_ = METHOD_NULL;
  status_code_ = HTTPFRAMING_STATUS_CODE_NULL;
}

HTTPFraming::~HTTPFraming(void) {
#if DEBUG_CLASS
  warnx("HTTPFraming::~HTTPFraming(void) called.");
#endif
  // At this time, we don't do anything.
}

// Copy constructor (needed for STL).
HTTPFraming::HTTPFraming(const HTTPFraming& src) 
    : uri_(src.uri_), msg_hdrs_(src.msg_hdrs_) {
#if DEBUG_CLASS
  warnx("HTTPFraming::HTTPFraming(const HTTPFraming& src) called.");
#endif

  major_ = src.major_;
  minor_ = src.minor_;
  msg_type_ = src.msg_type_;
  method_ = src.method_;
  status_code_ = src.status_code_;
}

// Assignment operator (needed for STL).
HTTPFraming& HTTPFraming::operator =(const HTTPFraming& src) {
#if DEBUG_CLASS
  warnx("HTTPFraming::operator =(const HTTPFraming& src) called.");
#endif

  major_ = src.major_;
  minor_ = src.minor_;
  msg_type_ = src.msg_type_;
  msg_hdrs_ = src.msg_hdrs_;
  method_ = src.method_;
  status_code_ = src.status_code_;
  uri_ = src.uri_;

  return *this;
}

#if 0
// Overloaded operator: equality functions.
int HTTPFraming::operator ==(const HTTPFraming& other)
{
  static string tmp_str;

  tmp_str = other.print();
  if (! strncasecmp(print(), tmp_str, strlen(print())))
    return 1;

  return 0;
}
#endif

// Accessors.
size_t HTTPFraming::hdr_len(void) const {
  return strlen(print_hdr(0).c_str());
}

// Routine to return the 'Content-Length' field-name.
size_t HTTPFraming::msg_len(void) const {
  vector<struct rfc822_msg_hdr>::const_iterator msg_hdr = msg_hdrs_.begin();
  while (msg_hdr != msg_hdrs_.end()) {
    if (strlen(msg_hdr->field_name.c_str()) == strlen(MIME_CONTENT_LENGTH)) {
      if (!strncasecmp(msg_hdr->field_name.c_str(), MIME_CONTENT_LENGTH, 
                       strlen(MIME_CONTENT_LENGTH))) {
        return strtol(msg_hdr->field_value.c_str(), (char**)NULL, 10);
      }
    }

    msg_hdr++;
  }

  // If we made it here, we didn't find a 'Content-Length' field.
  return 0;
}

// Routine to return the 'Content-Type' field-name.
string HTTPFraming::content_type(void) const {
  vector<struct rfc822_msg_hdr>::const_iterator msg_hdr = msg_hdrs_.begin();
  while (msg_hdr != msg_hdrs_.end()) {
    if (strlen(msg_hdr->field_name.c_str()) == strlen(MIME_CONTENT_TYPE)) {
      if (!strncasecmp(msg_hdr->field_name.c_str(), MIME_CONTENT_TYPE, 
                       strlen(MIME_CONTENT_TYPE))) {
        return msg_hdr->field_value;
      }
    }

    msg_hdr++;
  }

  // If we made it here, we didn't find a 'Content-Type' field.
  return "NULL-Content-Type";
}

// Routine to return the 'Transfer-Encoding' field-name.
string HTTPFraming::transfer_encoding(void) const {
  vector<struct rfc822_msg_hdr>::const_iterator msg_hdr = msg_hdrs_.begin();
  while (msg_hdr != msg_hdrs_.end()) {
    if (strlen(msg_hdr->field_name.c_str()) == strlen(MIME_TRANSFER_ENCODING)) {
      if (!strncasecmp(msg_hdr->field_name.c_str(), MIME_TRANSFER_ENCODING, 
                       strlen(MIME_TRANSFER_ENCODING))) {
        return msg_hdr->field_value;
      }
    }

    msg_hdr++;
  }

  // If we made it here, we didn't find a 'Transfer-Encoding' field.
  return "";
}

struct rfc822_msg_hdr HTTPFraming::msg_hdr(const char* field_name) const {
  struct rfc822_msg_hdr empty;
  if (field_name == NULL)
    return empty;

  vector<struct rfc822_msg_hdr>::const_iterator msg_hdr = msg_hdrs_.begin();
  while (msg_hdr != msg_hdrs_.end()) {
    if (strlen(msg_hdr->field_name.c_str()) == strlen(field_name)) {
      if (!strncasecmp(msg_hdr->field_name.c_str(), field_name, 
                       strlen(field_name))) {
        return *msg_hdr;
      }
    }

    msg_hdr++;
  }

  // If we made it here, we didn't find the field_name.
  return empty;
}

// Mutators.
void HTTPFraming::set_uri(const URL& uri) {
  uri_ = uri;
}

void HTTPFraming::set_method(const int method) {
  method_ = method;
}

void HTTPFraming::set_status_code(const int status_code) {
  status_code_ = status_code;
}

void HTTPFraming::set_connection(const int connection) {
  // First, see if we already have a message-header for "Connection".
  vector<struct rfc822_msg_hdr>::iterator msg_hdr = msg_hdrs_.begin();
  while (msg_hdr != msg_hdrs_.end()) {
    if (strlen(msg_hdr->field_name.c_str()) == strlen(HTTPFRAMING_CONNECTION)) {
      if (!strncasecmp(msg_hdr->field_name.c_str(), HTTPFRAMING_CONNECTION, 
                       strlen(HTTPFRAMING_CONNECTION))) {
        msg_hdr->field_value = connection ? kValueClose : "open";
        break;
      }
    }

    msg_hdr++;
  }

  if (msg_hdr == msg_hdrs_.end()) {
    // We did not find a "Connection" message-header, so add one.
    struct rfc822_msg_hdr tmp_msg_hdr;
    tmp_msg_hdr.field_name = HTTPFRAMING_CONNECTION;
    tmp_msg_hdr.field_value = connection ? kValueClose : "open";
    msg_hdrs_.push_back(tmp_msg_hdr);
  }
}

void HTTPFraming::clear(void) {
  major_ = HTTPFRAMING_VERSION_MAJOR;
  minor_ = HTTPFRAMING_VERSION_MINOR;
  msg_type_ = NOT_READY;
  method_ = METHOD_NULL;
  msg_hdrs_.clear();
  uri_.clear();
  status_code_ = 0;
}

// HTTPFraming manipulation.

// Routine to *pretty* print an HTTPFraming object.
string HTTPFraming::print(void) const {

  // The printed form will depend on what type of header we
  // are, i.e., RESPONSE | REQUEST.  The REQUEST header looks like:
  //
  // 	method_ URI HTTP-Version msg_len_ hdr_len_ connection_ type
  //
  // while the RESPONSE header looks like:
  //
  //	HTTP-Version status_code_ msg_len_ hdr_len_ connection_ type
  //
  // if neither is set, we simply return:
  //
  //	method_ status_code_ msg_len_ hdr_len_

  // TODO(aka) It's arguable that print() should *just* print the "start-line"!

  string tmp_str(SCRATCH_BUF_SIZE, '\0');

  if (method_ != METHOD_NULL) {
    // We are a REQUEST.
    snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, 
             "%s %s HTTP/%d.%d %lu %lu %s",
             method_names[method()], uri_.print().c_str(),
             major_, minor_, (unsigned long)msg_len(), (unsigned long)hdr_len(), 
             content_type().c_str());
  } else if (status_code_ != HTTPFRAMING_STATUS_CODE_NULL) {
    // We are a RESPONSE.
    snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE,
             "HTTP/%d.%d %d %lu %lu %s", 
             major_, minor_, status_code_, (unsigned long)msg_len(), (unsigned long)hdr_len(), 
             content_type().c_str());
  } else {
    snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%d %d %lu %lu", 
             method_, status_code_, (unsigned long)msg_len(), (unsigned long)hdr_len());
  }

  return tmp_str;
}

// Routine to print an HTTP status-line (dervived from the HTTPFraming
// object).
string HTTPFraming::print_start_line(void) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  if (method_ != METHOD_NULL) {
    // We are a request.
    snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s %s HTTP/%d.%d",
                 method_names[method()], 
                 uri_.print().c_str(), major_, minor_);
  } else {  // if (method != METHOD_NULL)
    // We are a response.
    const char* phrase = status_code_phrase(status_code());
    snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "HTTP/%d.%d %d %s",
                 major_, minor_, status_code(), phrase);
  }  // if else (method != METHOD_NULL)

  return tmp_str;
}

// Routine to print out the HTTP message-headers.
string HTTPFraming::print_msg_hdrs(void) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  // Try to *print out* all of the message-headers, one at a time ...
  int n = 0;
  vector<rfc822_msg_hdr>::const_iterator msg_hdr = msg_hdrs_.begin();
  while (msg_hdr != msg_hdrs_.end()) {
    // See if we have a *parameterized* message-header.
    if (msg_hdr->parameters.size()) {
      n = snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                   SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "%s: %s; ", 
                   msg_hdr->field_name.c_str(), msg_hdr->field_value.c_str());

      vector<struct rfc822_parameter>::const_iterator param = 
          msg_hdr->parameters.begin();
      while (param != msg_hdr->parameters.end()) {
        n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                      SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "%s=%s", 
                      param->key.c_str(), param->value.c_str());

        param++;
        if (param != msg_hdr->parameters.end())
          n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                        SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "; ");
        else
          n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                        SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "\r\n");
      }
    } else {
      n = snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                   SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "%s: %s\r\n", 
                   msg_hdr->field_name.c_str(), msg_hdr->field_value.c_str());
    }

    msg_hdr++;
  }

  // Sanity check that we had enough room in tmp_str.
  if (n >= SCRATCH_BUF_SIZE) {
    // TODO(aka) We need to resize tmp_str and try again!
    _LOGGER(LOG_WARNING, "HTTPFraming::print_msg_hdrs(): "
            "we used all of tmp_str's buffer (%ld) in making msg-hdrs!", 
            SCRATCH_BUF_SIZE);
  }

  return tmp_str;
}

// Routine to print out the HTTP Response or Request header.  Theo nly
// trick here is that we need to insert the empty line (CRLF) at the
// end of the message (as well as terminating the status-line with a
// CRLF).
string HTTPFraming::print_hdr(const int offset) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  int n = snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s\r\n%s\r\n",
                   print_start_line().c_str(), print_msg_hdrs().c_str());
  if (n >= SCRATCH_BUF_SIZE) {
    // TODO(aka) We need to resize tmp_str and try again!
    _LOGGER(LOG_WARNING, "HTTPFraming::print_hdr(): TODO(aka) "
            "scratch buffer size is %ld, but snprintf() returned %ld.",
            SCRATCH_BUF_SIZE, n);
  }

  if ((size_t)offset <= tmp_str.size())
    return tmp_str.substr(offset);  // just return what we want
  else 
    return tmp_str;
}

// Routine to set an HTTP header as a *request*.
void HTTPFraming::InitRequest(const int method, const URL& uri) {
  clear();

  msg_type_ = REQUEST;
  set_method(method);
  set_uri(uri);
}

// Routine to setup an HTTP header as a *response*.
void HTTPFraming::InitResponse(const int code, const int connection) {
  clear();

  msg_type_ = RESPONSE;
  set_status_code(code);
  set_connection(connection);
}

// Routine to (attempt to) build a HTTPFraming object (basically, the
// HTTP headers) from a char* buffer (e.g., the data returned from a
// network read(3) call).  If we succeed at building the full headers,
// i.e., we successfully parse up to the CRLFCRLF combo, then we
// remove the header from rbuf_ and return the boolean TRUE, so that
// the calling routine can process the message-body remaining in buf.
// However, if the sender encoded the message body using *chunking*,
// then this routine returns the NULL-terminated de-chunked message
// body in the buffer (chunked_msg_body) passed into this routine.
//
// This routine can set an ErrorHandler event.
bool HTTPFraming::InitFromBuf(const char* buf, const size_t len, 
                              const in_port_t default_port, size_t* bytes_used,
                              char** chunked_msg_body, 
                              size_t* chunked_msg_body_size) {
  clear();  // start from scratch

  if (buf == NULL) {
    error.Init(EX_SOFTWARE, "HTTPFraming::InitFromBuf(): buf is NULL");
    return false;
  }

  // Note, this routine (and any that it calls) will return false
  // until *sufficient* data is in the buffer to complete what ever
  // task was asked of it.  That is, for this top-level routine, we
  // must read the "CRLF" marking the end of the message header(s) to
  // *not* return false.  Moreover, if chunking was specified, then we
  // must read all of the message-body, as well.

  size_t n = len;  // set aside byte cnt
  const char* buf_ptr = buf;  // setup a walk pointer
  while (n > 0 && isspace(*buf_ptr)) {
    buf_ptr++;	// skip leading whitespace
    n--;
  }

  // See what type of HTTP message this is (and if we have enough
  // data, obviously).

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::InitFromBuf(): "
          "bytes_used: %ld, buf[%ld]: %s.", *bytes_used, len - n, buf_ptr);
#endif

  if (strlen(buf_ptr) <= strlen(kHTTPSlash))
    return false;  // not enough data yet

  // Proceed depending on the first element of the status-line ...
  bool ret_val = false;
  size_t parse_bytes_used = 0;
  if (strncasecmp(kMethodGet, buf_ptr, strlen(kMethodGet)) == 0)
    ret_val = ParseRequestHdr(buf_ptr, n, default_port, &parse_bytes_used);
  else if (strncasecmp(kMethodHead, buf_ptr, strlen(kMethodHead)) == 0) 
    ret_val = ParseRequestHdr(buf_ptr, n, default_port, &parse_bytes_used);
  else if (strncasecmp(kMethodPost, buf_ptr, strlen(kMethodPost)) == 0)
    ret_val = ParseRequestHdr(buf_ptr, n, default_port, &parse_bytes_used);
  else if (strncasecmp(kMethodPut, buf_ptr, strlen(kMethodPut)) == 0)
    ret_val = ParseRequestHdr(buf_ptr, n, default_port, &parse_bytes_used);
  else if (strncasecmp(kMethodDelete, buf_ptr, strlen(kMethodDelete)) == 0)
    ret_val = ParseRequestHdr(buf_ptr, n, default_port, &parse_bytes_used);
  else if (strncasecmp(kHTTPSlash, buf_ptr, strlen(kHTTPSlash)) == 0) 
    ret_val = ParseResponseHdr(buf_ptr, n, &parse_bytes_used,
                               chunked_msg_body, chunked_msg_body_size);
  else {
    // ERROR ...
    error.Init(EX_SOFTWARE, "HTTPFraming::InitFromBuf(): "
               "unknown status-line: %s", buf_ptr);
    return false;
  }

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::InitFromBuf(): after parse, "
          "ret_val: %d, bytes used: %ld, error flag: %d, buf[%ld]: %s.",
          ret_val, (len - n) + parse_bytes_used, error.Event(), 
          (len - ((len - n) + parse_bytes_used)), buf_ptr + parse_bytes_used);
#endif

  if (error.Event()) {
    // ParseRequestHdr() or ParseResponseHdr() failed, add our say and
    // return.

    error.AppendMsg("HTTPFraming::InitFromBuf(): ");
    return false;
  }

  if (!ret_val)
    return false;  // not enough data

  *bytes_used = (len - n) + parse_bytes_used;

  // Note, if the message-body was chunked, [SSL|TCP]Session should
  // add the de-chunked body back in rbuf_ and add a Content-Length
  // MsgHdr.

  return true;
}

// Routine to append a message-header to the HTTPFraming object.
//
// This routine can set an ErrorHandler event.
void HTTPFraming::AppendMsgHdr(const struct rfc822_msg_hdr& msg_hdr) {
  // First, make sure we don't already have this header ...
  vector<struct rfc822_msg_hdr>::iterator itr = msg_hdrs_.begin();
  while (itr != msg_hdrs_.end()) {
    if (strlen(itr->field_name.c_str()) == strlen(msg_hdr.field_name.c_str())) {
      if (!strncasecmp(itr->field_name.c_str(), msg_hdr.field_name.c_str(), 
                       strlen(msg_hdr.field_name.c_str()))) {
        // TODO(aka) Shouldn't this just erase the existing
        // message-header (so we can re-install after the while()
        // loop?

        error.Init(EX_SOFTWARE, "HTTPFraming::AppendMsgHdr(): TODO(aka) "
                   "Attempting to install %s, but already exists!", 
                   msg_hdr.field_name.c_str());
        return;
      }
    }

    itr++;
  }

  msg_hdrs_.push_back(msg_hdr);
}

// A convenience version of the previous routine, if your field does
// not require a parameter list.
//
// This routine can set an ErrorHandler event.
void HTTPFraming::AppendMsgHdr(const char* field_name, const char* field_value,
                               const char* key, const char* value) {
  if (field_name == NULL) {
    error.Init(EX_SOFTWARE, "HTTPFraming::AppendMsgHdr(): field_name is NULL");
    return;
  }
  if (field_value == NULL) {
    error.Init(EX_SOFTWARE, "HTTPFraming::AppendMsgHdr(): field_value is NULL");
    return;
  }

  // First, make sure we don't already have this header ...
  vector<struct rfc822_msg_hdr>::iterator msg_hdr = msg_hdrs_.begin();
  while (msg_hdr != msg_hdrs_.end()) {
    if (strlen(msg_hdr->field_name.c_str()) == strlen(field_name)) {
      if (!strncasecmp(msg_hdr->field_name.c_str(), 
                       field_name, strlen(field_name))) {
        error.Init(EX_SOFTWARE, "HTTPFraming::AppendMsgHdr(): TODO(aka) "
                   "Attempting to install %s, but already exists!", field_name);
        return;
      }
    }

    msg_hdr++;
  }

  struct rfc822_msg_hdr tmp_msg_hdr;
  tmp_msg_hdr.field_name = field_name;
  tmp_msg_hdr.field_value = field_value;
  if (key != NULL && value != NULL) {
    struct rfc822_parameter tmp_param;
    tmp_param.key = key;
    tmp_param.value = value;
    tmp_msg_hdr.parameters.push_back(tmp_param);
  }
  msg_hdrs_.push_back(tmp_msg_hdr);
}

// Routine to process a HTTP message header as a request.
//
// Note, this routine can set an ErrorHandler event.
bool HTTPFraming::ParseRequestHdr(const char* buf, const size_t len,
                                  const in_port_t default_port, 
                                  size_t* bytes_used) {
  if (buf == NULL) {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): buf is NULL");
    return 0;
  }

  // Note, the header is comprised of:
  //
  //	start-line CRLF
  //	*(message-header CRLF)
  //	CRLF
  //	
  // The message body would then follow the CRLF.
  //
  // Where start-line for a REQUEST is comprised of:
  //
  //	method_ SP request-URI SP HTTP-version CRLF

  char scratch_buffer[SCRATCH_BUF_SIZE];
  size_t n = len;  // set aside byte cnt

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseRequestHdr(): buf[%ld]: %s, %hu.",
          n, buf, default_port);
#endif

  const char* buf_ptr = buf;  // setup a walk pointer
  while (n > 0 && isspace(*buf_ptr)) {
    buf_ptr++;	// skip leading whitespace
    n--;
  }

  // At a minimum, a Request-Line will take 16b (and that's with a URI
  // of 3 chars!  So at least make sure we've got that much data.

  if (n < 16)
    return false;  // not enough data yet

  // Get method_ (based on sizeof method string).
  if (strncasecmp(kMethodGet, buf_ptr, strlen(kMethodGet)) == 0) {
    buf_ptr += strlen(kMethodGet);  // skip over "GET"
    n -= strlen(kMethodGet);
    set_method(GET);
  } else if (!strncasecmp(kMethodPut, buf_ptr, strlen(kMethodPut))) {
    buf_ptr += strlen(kMethodPut);  // skip over "PUT"
    n -= strlen(kMethodPut);
    set_method(PUT);
  } else if (!strncasecmp(kMethodHead, buf_ptr, strlen(kMethodHead))) {
    buf_ptr += strlen(kMethodHead);  // skip over "HEAD"
    n -= strlen(kMethodHead);
    set_method(HEAD);
  } else if (!strncasecmp(kMethodPost, buf_ptr, strlen(kMethodPost))) {
    buf_ptr += strlen(kMethodPost);  // skip over "POST"
    n -= strlen(kMethodPost);
    set_method(POST);
  } else if (!strncasecmp(kMethodDelete, buf_ptr, strlen(kMethodDelete))) {
    buf_ptr += strlen(kMethodDelete);  // skip over "DELETE"
    n -= strlen(kMethodDelete);
    set_method(DELETE);
  } else {
    // ERROR ...
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): "
               "unknown method: %s", buf_ptr);
    return false;
  }

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseRequestHdr(): method: %d, "
          "buf ptr (%ld): %s.",
          method_, (len - n), buf_ptr);
#endif

  // Get the URI.

  // We *should* be sitting on a SP (and we know we have data).
  if (*buf_ptr != ' ') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): "
               "expected SP, got char: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip SP
  if (--n == 0)
    return false;  // not enough data yet

  // TODO(aka) Note, until URL::set_Host() checks for a *complete*
  // host name, there is a small chance that we could get hosed here.

  size_t m = uri_.InitFromBuf(buf_ptr, n, default_port);
  if (m == 0 || ((n -= m) == 0))
    return false;  // not enough data yet
  buf_ptr += m;

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseRequestHdr(): URI: %s, "
          "buf[%ld]: %s.",
          uri_.print().c_str(), (len - n), buf_ptr);
#endif

  // We *should* be sitting on a SP (and we know we have data).
  if (*buf_ptr != ' ') {
    // TODO(aka) Hmm, an HTTP/0.9 request?
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): "
               "expected SP, got char: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip SP
  if (--n == 0)
    return false;  // not enough data yet

  // Get version.
  if (n > strlen(kHTTPSlash) &&
      strncasecmp(kHTTPSlash, buf_ptr, strlen(kHTTPSlash)) == 0) {
    buf_ptr += strlen(kHTTPSlash);  // skip over "HTTP/"
    n -= strlen(kHTTPSlash);
  } else {
    return false;  // not enough data yet
  }

  // Get the "major_" version number.
  char* ptr = &scratch_buffer[0];
  while (n > 0 && isdigit(*buf_ptr)) {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return false;  // not enough data yet
  *ptr = '\0';	// null terminate number
  major_ = strtol(scratch_buffer, (char**)NULL, 10);

  // We should be sitting on a '.' (and we know we have data).
  if (*buf_ptr != '.') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): TODO(aka) "
               "expected \'.\', got char: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip '.'
  if (--n == 0)
    return false;  // not enough data yet

  // Get the "minor" version number.
  ptr = &scratch_buffer[0];
  while (n > 0 && isdigit(*buf_ptr)) {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return false;  // not enough data yet
  *ptr = '\0';	// null terminate number
  minor_ = strtol(scratch_buffer, (char**)NULL, 10);

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseRequestHdr(): version: %d.%d, "
          "buf[%ld]: %s.",
          major_, minor_, (len - n), buf_ptr);
#endif

  if (major_ == 1 && minor_ == 1)
    _LOGGER(LOG_DEBUG, "HTTPFraming::Parse_Request_Hdr(): Received HTTP header version: %d.%d.", major_, minor_);
  else if (major_ == 1 && minor_ == 0)
    _LOGGER(LOG_DEBUG, "HTTPFraming::Parse_Request_Hdr(): Received HTTP header version: %d.%d.", major_, minor_);
  else if (major_ == 0 && minor_ == 9)
    _LOGGER(LOG_DEBUG, "HTTPFraming::Parse_Request_Hdr(): Received HTTP header version: %d.%d.", major_, minor_);
  else {
    error.Init(EX_SOFTWARE, "HTTPFraming::Parse_Request_Hdr(): "
               "received unknown HTTP header version: %d.%d", 
               major_, minor_);
    return false;
  }

  // End of Request-Line: expecting CRLF (anything else, not ready!)
  if (*buf_ptr != '\r') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): "
               "expected '\r', got: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip '\r'
  if (--n == 0)
    return false;  // not enough data yet

  if (*buf_ptr != '\n') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): "
               "expected '\n', got: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip '\n'
  if (--n == 0)
    return false;  // not enough data yet

  // At this point, we can either have a CRLF signifying end of 
  // "message headers" and *possible* start of "message body", or
  // we *should* see some "message headers".

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseRequestHdr(): "
          "after Request-Line(%ld): %s.", (len - n), buf_ptr);
#endif

  // If we have a message header, let's loop until start of empty-line ...
  while (*buf_ptr != '\r') {
    size_t m = ParseMsgHdr(buf_ptr, n);
    if (m == 0 || ((n -= m) == 0)) {
      msg_hdrs_.clear();
      return false;  // not enough data yet
    }
    buf_ptr += m;

#if DEBUG_PARSE
    _LOGGER(LOG_NOTICE, "HTTPFraming::ParseRequestHdr(): "
            "next msg-hdr(%ld): %s.", (len - n), buf_ptr);
#endif
  }

  // Note, any returns from here on out must clear msg_hdrs_.
 
  // We *should* be at the second CRLF at this point.
  if (*buf_ptr != '\r') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): "
               "expected '\r', got: %s at cnt: %ld", buf_ptr, (len - n));
    msg_hdrs_.clear();
    return false;
  }
  buf_ptr++;  // skip '\r'
  if (--n == 0) {
    msg_hdrs_.clear();
    return false;  // not enough data yet
  }

  if (*buf_ptr != '\n') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseRequestHdr(): "
               "expected '\n', got: %s at cnt: %ld", buf_ptr, (len - n));
    msg_hdrs_.clear();
    return false;
  }
  buf_ptr++;  // skip '\n'
  if (--n == 0) {
    msg_hdrs_.clear();
    return false;  // not enough data yet
  }

  // Okay, end of header, so mark location.
  msg_type_ = HTTPFraming::REQUEST;

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseRequestHdr(): buf[%ld]: %s.",
          (len - n), buf_ptr);
#endif
  *bytes_used = (len - n);  // amount of data used from buf

  return true;
}

// Routine to process a HTTP message header as a response.
//
// Note, this routine can set an ErrorHandler event.
bool HTTPFraming::ParseResponseHdr(const char* buf, const size_t len,
                                   size_t* bytes_used, char** chunked_msg_body, 
                                   size_t* chunked_msg_body_size) {
  if (buf == NULL) {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "buf is NULL");
    return false;
  }

  // Note, the header is comprised of:
  //
  //	start-line CRLF
  //	*(message-header CRLF)
  //	CRLF
  //	
  // The message body would then follow the CRLF.
  //
  // Where start-line for a RESPONSE is comprised of:
  //
  //	HTTP-version SP status-code SP reason-phrase CRLF
  
  char scratch_buffer[SCRATCH_BUF_SIZE];
  size_t n = len;  // set aside byte cnt

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseResponseHdr(): buf[%ld]: %s.",
          n, buf);
#endif

  const char* buf_ptr = buf;  // setup a walk pointer
  while (n > 0 && isspace(*buf_ptr)) {
    buf_ptr++;	// skip leading whitespace
    n--;
  }

  // Get version.
  if (n > strlen(kHTTPSlash) &&
      strncasecmp(kHTTPSlash, buf_ptr, strlen(kHTTPSlash)) == 0) {
    buf_ptr += strlen(kHTTPSlash);  // skip over "HTTP/"
    n -= strlen(kHTTPSlash);
  } else {
    return false;  // not enough data yet
  }

  // Get the "major_" version number.
  char* ptr = &scratch_buffer[0];
  while (n > 0 && isdigit(*buf_ptr)) {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return false;  // not enough data yet
  *ptr = '\0';	// null terminate number
  major_ = strtol(scratch_buffer, (char**)NULL, 10);

  // We should be sitting on a '.' (and we know we have data).
  if (*buf_ptr != '.') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "expected \'.\', got char: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip '.'
  if (--n == 0)
    return false;  // not enough data yet

  // Get the "minor" version number.
  ptr = &scratch_buffer[0];
  while (n > 0 && isdigit(*buf_ptr)) {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return false;  // not enough data yet
  *ptr = '\0';	// null terminate number
  minor_ = strtol(scratch_buffer, (char**)NULL, 10);

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseResponseHdr(): version: %d.%d, "
          "buf[%ld]: %s.",
          major_, minor_, (len - n), buf_ptr);
#endif

  if (major_ == 1 && minor_ == 1)
    _LOGGER(LOG_DEBUG, "HTTPFraming::ParseResponseHdr(): Received HTTP header version: %d.%d.", major_, minor_);
  else if (major_ == 1 && minor_ == 0)
    _LOGGER(LOG_DEBUG, "HTTPFraming::ParseResponseHdr(): Received HTTP header version: %d.%d.", major_, minor_);
  else if (major_ == 0 && minor_ == 9)
    _LOGGER(LOG_DEBUG, "HTTPFraming::ParseResponseHdr(): Received HTTP header version: %d.%d.", major_, minor_);
  else {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "received unknown HTTP header version: %d.%d", 
               major_, minor_);
    return false;
  }

  // We *should* be sitting on a space (and we have data).
  if (*buf_ptr != ' ') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "expected SP, got char: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip SP
  if (--n == 0)
    return false;  // not enough data yet

  // Get the status code.
  ptr = &scratch_buffer[0];
  while (n > 0 && isdigit(*buf_ptr)) {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return false;  // not enough data yet
  *ptr = '\0';	// null terminate code
  set_status_code(strtol(scratch_buffer, (char**)NULL, 10));

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseResponseHdr(): "
          "status code %d, buf[%ld]: %s.",
          status_code_, (len - n), buf_ptr);
#endif

  // We *should* be sitting on a space (and we have data).
  if (*buf_ptr != ' ') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "expected SP, got char: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip SP
  if (--n == 0)
    return false;  // not enough data yet

  // Get the status phrase.
  ptr = &scratch_buffer[0];
  while (n > 0 && *buf_ptr != '\r') {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return false;  // not enough data yet
  *ptr = '\0';	// null terminate phrase
  string status_phrase = scratch_buffer;

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseResponseHdr(): "
          "status phrase %s, buf[%ld]: %s.",
          status_phrase.c_str(), (len - n), buf_ptr);
#endif

  // End of Status-Line: expecting CRLF (anything else, not ready!)
  if (*buf_ptr != '\r') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "expected '\r', got: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip '\r'
  if (--n == 0)
    return false;  // not enough data yet

  if (*buf_ptr != '\n') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "expected '\n', got: %s at cnt: %ld", buf_ptr, (len - n));
    return false;
  }
  buf_ptr++;  // skip '\n'
  if (--n == 0)
    return false;  // not enough data yet

  // At this point, we can either have a CRLF signifying end of 
  // "message headers" and *possible* start of "message body", or
  // we *should* see some "message headers".

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseResponseHdr(): "
          "after status-line(%ld): %s.", (len - n), buf_ptr);
#endif

  // If we have a message header, let's loop until start of CRLF ...
  while (*buf_ptr != '\r') {
    size_t m = ParseMsgHdr(buf_ptr, n);
    if (m == 0 || ((n -= m) == 0)) {
      if (error.Event())
        error.AppendMsg("HTTPFraming::ParseResponseHdr(): ");

      msg_hdrs_.clear();
      return false;  // not enough data yet
    }
    buf_ptr += m;

#if DEBUG_PARSE
    _LOGGER(LOG_NOTICE, "HTTPFraming::ParseResponseHdr(): "
            "next msg-hdr(%ld): %s.", (len - n), buf_ptr);
#endif
  }

  // Note, any non-success returns from here on out must clear msg_hdrs_.
 
  // We *should* be at the \r in the second CRLF at this point.
  buf_ptr++;  // skip '\r'
  if (--n == 0) {
    msg_hdrs_.clear();
    return false;  // not enough data yet
  }
  if (*buf_ptr != '\n') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseResponseHdr(): TODO(aka) "
               "expected '\n', got: %s at cnt: %ld", buf_ptr, (len - n));
    msg_hdrs_.clear();
    return false;
  }
  buf_ptr++;  // skip '\n'
  if (--n == 0)
    return false;  // not enough data yet

  // End of message header.
  msg_type_ = HTTPFraming::RESPONSE;

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseResponseHdr(): "
          "status code %d, buf[%ld]: %s.",
          status_code_, (len - n), buf_ptr);
#endif

  // Okay, RFC 2616 says that the server does *not* need to use
  // 'Content-length' to signify additional data *if*
  // 'Transfer-Encoding' is set to chunked.  Unfortunately, the rest
  // of the this library uses 'Content-Length' to know when we have a
  // complete message.  Thus, if *chunked* is set, we need to keep
  // reading *now* to process the chunks.

  static const char* kMimeChunked = MIME_CHUNKED;
  if (strlen(kMimeChunked) == strlen(transfer_encoding().c_str()) &&
      !strncasecmp(kMimeChunked, transfer_encoding().c_str(), 
                   strlen(kMimeChunked))) {
    // Great, there's a chunked message-body, joy.
    size_t chunked_bytes_used = 0;
    if (!ParseChunkedMsgBody(buf_ptr, len - n, &chunked_bytes_used,
                             chunked_msg_body, chunked_msg_body_size)) {
      if (error.Event())
        error.AppendMsg("HTTPFraming::ParseResponseHdr(): ");

      msg_hdrs_.clear();
      return false;  // either not enough data or we encountered an error
    }

    n -= chunked_bytes_used;
  }

  *bytes_used = len - n;  // amount of data we used in buf

  return true;
}

// Routine to process one "field" within the *message headers*
// section, within the HTTP header.
//
// Note, this routine can set an ErrorHandler event.
size_t HTTPFraming::ParseMsgHdr(const char* buf, const size_t len) {
  if (buf == NULL) {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseMsgHdr(): TODO(aka) "
               "buf is NULL");
    return 0;
  }

  // Note, the line should look like:
  //
  //   Field-Name COLON field-value *(SEMICOLON parameter) CRLF
  //
  // Where 'parameter' is a *optional* semicolon-separated list:
  //
  //    key EQUALSIGN value

  if (strlen(buf) == 0)
    return 0;  // no work to do

  char scratch_buffer[SCRATCH_BUF_SIZE];
  struct rfc822_msg_hdr tmp_msg_hdr;
  size_t n = len;  // set aside byte cnt

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseMsgHdr(): buf[%ld]: %s.",
          (len - n), buf);
#endif

  const char* buf_ptr = buf;  // setup a walking pointer

  // TODO(aka) We need a test here to look for leading whitespace,
  // which could (conceivably) mark a continuation line from the
  // previous field value.

  if (isspace(*buf_ptr)) {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseMsgHdr(): TODO(aka) "
               "found space signaling line continuation at buf[%ld]: %s",
               (len - n), buf_ptr);
    return 0;
  }

  // Get 'field' name.
  char* ptr = &scratch_buffer[0];
  while (n > 0 && *buf_ptr != ':') {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return 0;  // not enough data yet
  *ptr = '\0';	// null terminate 'field' name
  tmp_msg_hdr.field_name = scratch_buffer;

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseMsgHdr(): name %s, buf[%ld]: %s.",
          tmp_msg_hdr.field_name.c_str(), (len - n), buf_ptr);
#endif

  // We're sitting on a ':', skip it.
  buf_ptr++;
  if (--n == 0)
    return 0;  // not enough data yet
		
  while (n > 0 && isspace(*buf_ptr)) {
    buf_ptr++;  // skip whitespace
    n--;
  }
  if (n == 0)
    return 0;  // not enough data yet

  // Grab 'field-value'.
  ptr = &scratch_buffer[0];
  while (n > 0 && *buf_ptr != ';' && *buf_ptr != '\r') {
    *ptr++ = *buf_ptr++;
    n--;
  }
  if (n == 0)
    return 0;  // not enough data yet
  *ptr = '\0';  // null terminate 'field' value
  tmp_msg_hdr.field_value = scratch_buffer;

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseMsgHdr(): value %s, buf[%ld]: %s.",
          tmp_msg_hdr.field_value.c_str(), (len - n), buf_ptr);
#endif

  // Now, we can either be at the CRFLF or the start of parameter, if
  // we're not at the CRLF, loop until we've read all parameters ...

  while (*buf_ptr != '\r') {
    buf_ptr++;  // skip ';'
    if (--n == 0)
      return 0;  // not enough data yet
		
    while (n > 0 && isspace(*buf_ptr)) {
      buf_ptr++;  // skip whitespace
      n--;
    }
    if (n == 0)
      return 0;  // not enough data yet

    // Grab our parameter
    struct rfc822_parameter tmp_param;

    // First, get the key.
    ptr = &scratch_buffer[0];
    while (n > 0 && *buf_ptr != '=') {
      *ptr++ = *buf_ptr++;
      n--;
    }
    if (n == 0)
      return 0;  // not enough data yet
    *ptr = '\0';  // null terminate key
    tmp_param.key = scratch_buffer;

#if DEBUG_PARSE
    _LOGGER(LOG_NOTICE, "HTTPFraming::ParseMsgHdr(): key %s, buf[%ld]: %s.",
            tmp_param.key.c_str(), (len - n), buf_ptr);
#endif

    buf_ptr++;  // skip '='
    if (--n == 0)
      return 0;  // not enough data yet

    while (n > 0 && isspace(*buf_ptr)) {
      buf_ptr++;  // skip whitespace
      n--;
    }
    if (n == 0)
      return 0;  // not enough data yet

    // Next, get the value.
    ptr = &scratch_buffer[0];
    while (n > 0 && *buf_ptr != ';' && *buf_ptr != '\r') {
      *ptr++ = *buf_ptr++;
      n--;
    }
    if (n == 0)
      return 0;  // not enough data yet
    *ptr = '\0';  // null terminate key
    tmp_param.value = scratch_buffer;

#if DEBUG_PARSE
    _LOGGER(LOG_NOTICE, "HTTPFraming::ParseMsgHdr(): value %s, buf[%ld]: %s.",
            tmp_param.value.c_str(), (len - n), buf_ptr);
#endif

    tmp_msg_hdr.parameters.push_back(tmp_param);

    while (n > 0 && (*buf_ptr == '\t' || *buf_ptr == ' ')) {
      buf_ptr++;  // skip whitespace (but not \r\n)
      n--;
    }
    if (n == 0)
      return 0;  // not enough data yet
  }  // while (*buf_ptr != '\r') {

  // Now we're sitting on the \r in our CRLF, skip it.
  buf_ptr++;
  if (--n == 0)
    return 0;
  if (*buf_ptr != '\n') {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseMsgHdr(): TODO(aka) "
               "expected '\n', got char: %s at cnt: %ld", buf_ptr, (len - n));
    return 0;
  }

  // Not necessary to increment buf_ptr here.
  n--;  // skip *expected* '\n'

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseMsgHdr(): END buf[%ld]: %s.",
          (len - n), buf_ptr);
#endif

  // And before returning, make sure we can support *the*
  // 'message-header' that was just set ...

#if 0  // TODO(aka) It's up to the caller to decide if it can support any MIME type ...
  // XXX TODO(aka) Add strlen() checks before strncasecmp() checks ...
  if (!strncasecmp(tmp_msg_hdr.field_name.c_str(),
                   MIME_CONTENT_TYPE, strlen(MIME_CONTENT_TYPE)) &&
      (strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_TEXT_PLAIN, strlen(MIME_TEXT_PLAIN)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_TEXT_XML, strlen(MIME_TEXT_XML)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_IMAGE_GIF, strlen(MIME_IMAGE_GIF)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_IMAGE_PNG, strlen(MIME_IMAGE_PNG)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_VIDEO_MP4, strlen(MIME_VIDEO_MP4)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_VIDEO_QUICKTIME, strlen(MIME_VIDEO_QUICKTIME)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_VIDEO_MPEG, strlen(MIME_VIDEO_MPEG)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_VIDEO_OGG, strlen(MIME_VIDEO_OGG)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_APP_TAR, strlen(MIME_APP_TAR)) &&
       strncasecmp(tmp_msg_hdr.field_value.c_str(),
                   MIME_BINARY, strlen(MIME_BINARY))))
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseMsgHdr(): TODO(aka) "
               "Unsupported \'content-type\': %s", 
               tmp_msg_hdr.field_value.c_str());
#endif

  msg_hdrs_.push_back(tmp_msg_hdr);

  return (len - n);
}

// Routine to slurp in a *chunked* message-body.
bool HTTPFraming::ParseChunkedMsgBody(const char* buf, const size_t len,
                                      size_t* bytes_used, char** msg_body,
                                      size_t* msg_body_size) const {
  *bytes_used = 0;
  if (buf == NULL || *msg_body == NULL) {
    error.Init(EX_SOFTWARE, "HTTPFraming::ParseChunkedMsgBody(): TODO(aka) "
               "buf or our msg_body buffer is NULL");
    return false;
  }

  // RFC 2616 defines a chunked msg-body as such:
  //
  //       Chunked-Body   = *chunk
  //                        last-chunk
  //                        trailer
  //                        CRLF
  //
  //       chunk          = chunk-size [ chunk-extension ] CRLF
  //                        chunk-data CRLF
  //       chunk-size     = 1*HEX
  //       last-chunk     = 1*("0") [ chunk-extension ] CRLF
  //
  //       chunk-extension= *( ";" chunk-ext-name [ "=" chunk-ext-val ] )
  //       chunk-ext-name = token
  //       chunk-ext-val  = token | quoted-string
  //       chunk-data     = chunk-size(OCTET)
  //       trailer        = *(entity-header CRLF)

  char scratch_buffer[SCRATCH_BUF_SIZE];
  size_t n = len;  // set aside byte cnt of buf
  size_t msg_body_cnt = 0;  // end (or count) of data in msg_body

#if DEBUG_PARSE
  _LOGGER(LOG_NOTICE, "HTTPFraming::ParseChunkedMsgBody(): "
          "On enter, buf[%ld]: %s.", n, buf);
#endif

  const char* buf_ptr = buf;  // setup a walk pointer
  while (n > 0 && isspace(*buf_ptr)) {
    buf_ptr++;	// skip leading whitespace
    n--;
  }

  // Loop over each chunk of data ...
  while (n > 0) {
    // Get the chunk size.
    char* scratch_ptr = &scratch_buffer[0];
    while (n > 0 && *buf_ptr != '\r') {
      *scratch_ptr++ = *buf_ptr++;
      n--;
    }
    if (n == 0) {
      return false;  // not enough data yet
    }
    *scratch_ptr = '\0';  // null terminate size
    long chunk_size = strtol(scratch_buffer, (char**)NULL, 16);

    // We should be sitting on CRLF, so skip over them.
    if (*buf_ptr != '\r') {
      error.Init(EX_SOFTWARE, "HTTPFraming::ParseChunkedMsgBody(): TODO(aka) "
                 "expected '\r', got: %s at cnt: %ld", buf_ptr, (len - n));
      return false;
    }
    buf_ptr++;  // skip '\r'
    if (--n == 0) {
      return false;  // not enough data yet
    }
    if (*buf_ptr != '\n') {
      error.Init(EX_SOFTWARE, "HTTPFraming::ParseChunkedMsgBody(): TODO(aka) "
                 "expected '\n', got: %s at cnt: %ld", buf_ptr, (len - n));
      return false;
    }
    buf_ptr++;  // skip '\n'
    if (--n == 0) {
      return false;  // not enough data yet
    }

#if DEBUG_PARSE
    _LOGGER(LOG_NOTICE, "HTTPFraming::ParseChunkedMsgBody(): "
            "chunk size: %ld, buf[%ld]: %s.",
            chunk_size, (len - n), buf_ptr);
#endif

    // If chunk-size == 0, we're all done.
    if (chunk_size == 0) {
      *bytes_used = (len - n);  // report how much of buf we used
      *(*msg_body + msg_body_cnt) = '\0';
      return true;
    }

    // Make sure we have enough room in our passed in storage.
    if (chunk_size + msg_body_cnt >= *msg_body_size) {
      // Need to realloc.
      size_t new_size = (SCRATCH_BUF_SIZE > chunk_size) ?
          (*msg_body_size + SCRATCH_BUF_SIZE) :
          (*msg_body_size + chunk_size + 1);
      if ((*msg_body = (char*)realloc(*msg_body, new_size)) == NULL) {
        error.Init(EX_OSERR,  "HTTPFraming::ParseChunkedMsgBody(): "
                   "realloc(3) failed for %ulb", new_size);
        return false;
      }

      *msg_body_size = new_size;
    }

    // We slurp up data and put in our passed in storage.
    while (n > 0 && *buf_ptr != '\r') {
      *(*msg_body + msg_body_cnt++) = *buf_ptr++;
      n--;
    }
    if (n == 0) {
      return false;  // not enough data yet
    }

    // We should be sitting on CRLF, so skip over them.
    if (*buf_ptr != '\r') {
      error.Init(EX_SOFTWARE, "HTTPFraming::ParseChunkedMsgBody(): TODO(aka) "
                 "expected '\r', got: %s at cnt: %ld", buf_ptr, (len - n));
      return false;
    }
    buf_ptr++;  // skip '\r'
    if (--n == 0) {
      return false;  // not enough data yet
    }
    if (*buf_ptr != '\n') {
      error.Init(EX_SOFTWARE, "HTTPFraming::ParseChunkedMsgBody(): TODO(aka) "
                 "expected '\n', got: %s at cnt: %ld", buf_ptr, (len - n));
      return false;
    }
    buf_ptr++;  // skip '\n'

#if DEBUG_PARSE
    _LOGGER(LOG_NOTICE, "HTTPFraming::ParseChunkedMsgBody(): "
            "chunk size: %ld, buf[%ld]: %s.",
            chunk_size, (len - n), buf_ptr);
#endif
  }

  // Shouldn't reach!
  return false;
}

// Routine to report if the REQUEST is for a WSDL service.
bool HTTPFraming::IsWSDLRequest(void) const {
  // TODO(aka) I think we need to check the path to see if "wsdl" is in it ...

  return false;
}
