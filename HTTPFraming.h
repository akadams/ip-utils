// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef HTTPFRAMING_H_
#define HTTPFRAMING_H_

#include <sys/types.h>

#include <netinet/in.h>

#include <string>
#include <list>
#include <vector>
using namespace std;

#include "ErrorHandler.h"
#include "URL.h"
#include "RFC822MsgHdr.h"
#include "MIMEFraming.h"      // just to get media-types

#define HTTPFRAMING_VERSION_MAJOR 1
#define HTTPFRAMING_VERSION_MINOR 1

#define HTTPFRAMING_STATUS_CODE_NULL 0
#define HTTPFRAMING_DEFAULT_HDR_SIZE (1024 * 4)

#define HTTPFRAMING_SCHEME "http"

const size_t kHTTPMsgBodyMaxSize = (1024 * 4);

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.

// Message-header field types.
#define HTTPFRAMING_CONNECTION "Connection"

// Non-class specific utilities.
const char* status_code_phrase(const int status_code);
const char* method_name(const int method);
const int method_id(const char* method_name);
const char* start_line_name(const int type);

/** Class for managing HTTP-framed messages.
 *
 *  The HTTPFraming class builds and parses HTTP headers, i.e., all
 *  the information up to the second '\r\n' (where the message-body
 *  would normally begin).
 *
 *  RCSID: $Id: HTTPFraming.h,v 1.2 2012/05/10 17:50:46 akadams Exp $
 *
 *  @author Andrew K. Adams <akadams@psc.edu>
 */
class HTTPFraming {
 public:
  /** Constructor.
   *
   */
  HTTPFraming(void);

  /** Destructor.
   *
   */
  virtual ~HTTPFraming(void);

  /** Copy constructor, needed for use with STL.
   *
   */
  HTTPFraming(const HTTPFraming& src);

  /** Assignment operator, needed for use with STL.
   *
   */
  HTTPFraming& operator =(const HTTPFraming& src);
  
  // Accessors.
  int msg_type(void) const { return msg_type_; }
  int method(void) const { return method_; }
  URL uri(void) const { return uri_; }
  int status_code(void) const { return status_code_; }
  vector<rfc822_msg_hdr> msg_hdrs(void) const { return msg_hdrs_; }
  
  /** Routine to return the HTTP framing header length.
   *
   *  @param abs_path a bool to signal if we want an abs_path or absoluteURI
   */
  size_t hdr_len(const bool abs_path) const;

  /** Routine to return the 'Content-Length' field-value.
   *
   */
  size_t msg_len(void) const;

  /** Routine to return the 'Content-Type' field-value.
   *
   */
  string content_type(void) const;

  /** Routine to return the 'Transfer-Encoding' field-value.
   *
   *  This is useful, since RESPONSE message (according to RFC 2616)
   *  don't need to set a 'Content-Length' when including a
   *  message-body.  Joy.
   */
  string transfer_encoding(void) const;

  /** Routine to return a specific message-header.
   *
   */
  struct rfc822_msg_hdr msg_hdr(const char* field_name) const;

  // Mutators.
  void set_uri(const URL& uri);
  void set_method(const int method);
  void set_status_code(const int status_code);
  void clear(void);
  void set_connection(int connection);

  // HTTP manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** Routine to print the HTTP start-line from the object.
   *
   *  The start-line is either the request-line in a REQUEST or the
   *  status-line in a RESPONSE.
   *
   *  @param abs_path a bool to signal if we want an abs_path or absoluteURI
   *  @return a string housing the start-line
   */
  string print_start_line(const bool abs_path) const;

  /** Routine to print the HTTP message-headers contained within the object.
   *
   */
  string print_msg_hdrs(void) const;

  /** Routine to print the HTTP header.
   *
   *  The print_hdrs() routine calls print_start_line() and
   *  print_msg_hdrs() to get the work done.
   *
   *  @param offset is a control to allow you to print-out only the
   *  message-headers
   *  @param abs_path a bool to signal if we want an
   *  abs_path or absoluteURI @return a string housing the start-line
   */
  string print_hdr(const int offset, const bool abs_path) const;

  /** Routine to initalize an HTTPFraming object as a REQUEST message.
   *
   */
  void InitRequest(const int method, const URL& uri);

  /** Routine to initialize an HTTPFraming objecct as a RESPONSE message.
   *
   */
  void InitResponse(const int code, const int connection);

  /** Parse (or initialize) a char* stream into an HTTPFraming object.
   *
   *  This routine takes a pointer to a character stream and attempts
   *  to parse the stream into an HTTPFraming object.  The amount of
   *  data used to create the HTTPFraming object is returned by the
   *  routine (0 if the routine is unable to *completely* build the
   *  HTTPFraming object).
   *
   *  TOOD(aka) Why do we need a default port?  (I think for URL, but
   *  not sure.)
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   *  @param buf a char* stream
   *  @param len a size_t specifying the size of buf
   *  @param default_port is an in_port_t to act as a default, if none is found
   *  @param bytes_used a size_t* showing how much data from buf was used
   *  @param chunked_msg_body a char** to hold the message-body if chunking set
   *  @param chunked_msg_body_size a size_t* showing current size of chunk buffer
   *  @return a size_t showing how much data from buf was used (or 0)
   */
  bool InitFromBuf(const char* buf, const size_t len, 
                   const in_port_t default_port, size_t* bytes_used,
                   char** chunked_msg_body, size_t* chunked_msg_body_size);

  /** Routine to append a message-header to the HTTPFraming object.
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   */
  void AppendMsgHdr(const struct rfc822_msg_hdr& msg_hdr);

  /** Routine to append a message-header to the HTTPFraming object.
   *
   *  This is a convenience version of the above routine, if you know
   *  that your field does not need a parameter list.
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   */
  void AppendMsgHdr(const char* field_name, const char* field_value,
                    const char* key, const char* value);

  /** Routine to parse a char* stream as an HTTPFraming REQUEST header.
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  TODO(aka) There seems to be some confusion as to whether the
   *  Parse* routines should return the amount of data used from the
   *  const buf, or if they should return a bool showing success or
   *  not (and setting the amount of data used via a parameter var
   *  ...).
   *
   *  @see ErrorHandler Class
   *  @param buf a char* stream
   *  @param len a size_t specifying the size of buf
   *  @param bytes_used a size_t showing how much data from buf was used (or 0)
   *  @return a bool showing successful parsing
   */
  bool ParseRequestHdr(const char* buf, const size_t len, 
                       const in_port_t default_port, size_t* bytes_used);

  /** Routine to parse a char* stream as an HTTPFraming RESPONSE header.
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   *  @param buf a char* stream
   *  @param len a size_t specifying the size of buf
   *  @param bytes_used a size_t showing how much data from buf was used
   *  @param chunked_msg_body a char** to hold the message-body if chunking set
   *  @param chunked_msg_body_size a size_t* showing current size of chunk buffer
   *  @return a bool showing successful parsing
   */
  bool ParseResponseHdr(const char* buf, const size_t len, size_t* bytes_used,
                        char** chunked_msg_body, size_t* chunked_msg_body_size);

  /** Routine to parse a char* stream as an HTTP message-header.
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   *  @param buf a char* stream
   *  @param len a size_t specifying the size of buf
   *  @return a size_t showing how much data from buf was used (or 0)
   */
  size_t ParseMsgHdr(const char* buf, const size_t len);

  /** Routine to parse a char* stream as an HTTP chunked message-body.
   *
   *  Any chunked data found in the message-body will be placed,
   *  chunk-at-a-time into the passed in msg_body buffer.  Upon
   *  reading a 0-sized chunk, the msg_body buffer will be
   *  null-terminated.  Thus, the amount of data within msg_body can
   *  be determined using strlen().
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   *  @param buf a char* stream
   *  @param len a size_t specifying the size of buf
   *  @param bytes_used a size_t showing how much data from buf was used (or 0)
   *  @param msg_body a char** to a buffer to hold the de-chunked msg_body
   *  @param msg_body_len a size_t* that reports the current size of above buffer
   *  @return
   */
  bool ParseChunkedMsgBody(const char* buf, const size_t len, size_t* bytes_used,
                           char** msg_body, size_t* msg_body_len) const;

  // Boolean checks.
  bool IsWSDLRequest(void) const;

  // Flags.
  enum { NOT_READY, REQUEST, RESPONSE, READY };
  enum { METHOD_NULL, GET, HEAD, POST, PUT, 
         DELETE, TRACE, CONNECT, OPTIONS };
  enum { OPEN, CLOSE };
  // enum HTTP_VERSIONS { NONE, 0_9, 1_0, 1_1, };

 protected:
  // Data members.
  int msg_type_;	    // NOT_READY | REQUEST | RESPONSE
  int major_;               // major version number
  int minor_;		    // minor version number
  int method_;		    // GET | HEAD | POST | DELETE
  int status_code_;	    // response status code
  URL uri_;		    // URL embedded in start-line of a request

  vector<struct rfc822_msg_hdr> msg_hdrs_;  // message-headers

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.
  int operator ==(const HTTPFraming& other) const;
};


#endif  /* #ifndef HTTPFRAMING_H_ */
