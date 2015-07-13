// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef MSGHDR_H_
#define MSGHDR_H_

#include <sys/types.h>

#include <stdint.h>

#include <string>
using namespace std;

#include "BasicFraming.h"
#include "HTTPFraming.h"
#include "MIMEFraming.h"

extern uint16_t msg_id_hash;  // declaration of global msg_id (found
                              // in MsgHdr.cc)

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.

// Non-class specific utilities.


/** Storage class to hold all possible framing headers.
 *
 *  This class is simply a container for all possible framing types a
 *  message can have.  It is used by MsgHdr as the storage for the
 *  framing header currently in use.
 *
 *  @see MsgHdr
 *  @see HTTPFraming
 */
class HdrStorage {
 public:
  /** Constructor.
   *
   */
  HdrStorage(void);

  /** Destructor.
   *
   */
  virtual ~HdrStorage(void);

  /** Copy constructor (needed for STL).
   *
   */
  HdrStorage(const HdrStorage& src);

  /** Assignment operator (needed for STL).
   *
   */
  HdrStorage& operator =(const HdrStorage& src);

  // Mutators.
  void set_basic(const struct BasicFramingHdr& basic);
  void set_http(const HTTPFraming& http);
  void clear(void);

  friend class MsgHdr;

 protected:
  struct BasicFramingHdr basic_;
  HTTPFraming http_;

 private:
  // Dummy declaration for equality operator.
  int operator ==(const HdrStorage& other) const;
};


/** Class to act as a handler for the framing used in network communications.
 *
 *  The MsgHdr class acts as an opaque container for the type of
 *  framing used in a network session; certain framing information is
 *  necessary for *session managing* classes, e.g., TCPSession, to
 *  process messages (e.g., to know when the message is complete), but
 *  additional application layer information stored in the framing
 *  header, e.g., Content-Type, is not necessary for the session
 *  manager -- in fact, that information may not be used until the
 *  message is processed by the application layer.  Thus, MsgHdr must
 *  be aware of all possible message framing protocols, e.g., HTTP,
 *  MIME, etc., that the network session (e.g., TCPSession) will use,
 *  but in only needs to provide a finite absraction to those classes
 *  for the session managers (providing that the framing classes all
 *  use the same API!).
 *
 *  MsgHdr (and event-loop processing code) only needs to operate in
 *  two modes, regardless of how many type of framing we have.  That
 *  is, class-based or struct-based modes.  Class-based mode simply
 *  requires the higher-layer code like the event-loop to use a
 *  member-function or method of the framing class to accomplish some
 *  task, e.g., use the class' copy constructors.  In struct-based
 *  mode we use memory functions to operate on the framing struct,
 *  e.g., memcpy(). In short, HTTPFraming would be class-based, while
 *  BasicFraming would be struct-based.  It is imporant that when a
 *  user adds a framing class to MsgHdr they set the correct type of
 *  operation mode in IsClassBased().
 *
 *  TODO(aka) At some point we should chnage struct framing_hdr into a
 *  Class.  Additionally, it would probably be nice to be able to
 *  return whatever framing header we had via a void* in the getter
 *  funtion(s), so that code use higher layer classes, e.g.,
 *  TCPSession, could actually use the header directly
 *  "tcp_session.hdr().print_status()".  Currently, the higher-layer
 *  codes has to get a copy of the struct, e.g., "HTTPFraming http_hdr
 *  = tcp_session.hdr();" and then access the HTTP information.
 *  Moreover, if you want to change any information within the struct
 *  via TCPSession, you need to overwrite the entire struct in MsgHdr.
 *
 *  If this was a Java class, as opposed to a C++ class, it would
 *  probably be called FramingWrapper or some such ... it's arguable
 *  whether that is a more descriptive name.
 *
 *  Note, it is expected that as framing classes get added to this
 *  library, this class will be updated to reflect that.  
 *  
 *  RCSID: $Id: MsgHdr.h,v 1.1 2012/02/03 13:17:10 akadams Exp $
 *
 *  @see HTTPFraming
 *  @see MIMEFraming
 *
 *  @author Andrew K. Adams <akadams@psc.edu>
 */
class MsgHdr {
 public:
  /** Constructor.
   *
   */
  //MsgHdr(void);

  /** Constructor.
   *
   */
  MsgHdr(void);

  /** Constructor.
   *
   */
  explicit MsgHdr(const uint8_t type);

  /** Destructor.
   *
   */
  virtual ~MsgHdr(void);

  /** Copy constructor (needed for STL).
   *
   */
  MsgHdr(const MsgHdr& src);

  /** Assignment operator (needed for STL).
   *
   */
  MsgHdr& operator =(const MsgHdr& src);

  // Accessors.
  uint8_t type(void) const { return type_; }
  uint16_t msg_id(void) const { return msg_id_; }
  HdrStorage hdr(void) const { return hdr_; }

  /** Routine to get the framing header length.
   *
   *  @return a size_t specifying the framing-header length
   */
  size_t hdr_len(void) const;

  /** Routine to get the message-body length (i.e., msg len minus the header).
   *
   *  @return a size_t specifying the message-body length
   */
  size_t body_len(void) const;

  /** Routine to return the basic framing header.
   *
   *
   *  @return a struct BasicFramingHdr copy of the header
   */
  struct BasicFramingHdr basic_hdr(void) const;

  /** Routine to return the HTTP framing header.
   *
   *  @return a HTTPFraming class copy of the header
   */
  HTTPFraming http_hdr(void) const;

  /** Routine to return the *extension* of the media type.
   *
   *  We return what the extension should be based on the media-type
   *  of the contents within the message, e.g., a text/xml media type
   *  would return "xml".
   *
   *  return a string holding either the media type or "", if not found
   */
  string GetMediaTypeExt(void);

  // Mutators.
  void set_msg_id(uint16_t msg_id);
  void set_type(uint8_t type);
  void set_hdr(const HdrStorage& hdr);
  void clear(void);

  // MsgHdr manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** A routine to write the header to a buffer.
   *
   *  This routine relies should produce a buffer suitable to act as
   *  the message-header in a message.
   */
  string print_hdr(const int offset) const;

  /** Routine to initialize the object using a struct BasicFramingHdr.
   *
   *  Work that is beyond what is expected within the constructor is
   *  done in here.
   *
   *  Note, this routine overloaded.  Additionally, it can set an
   *  ErrorHandler event if it encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   */
  void Init(const uint16_t msg_id, const struct BasicFramingHdr& hdr);

  /** Routine to initialize the object using a HTTPFraming object.
   *
   *  Work that is beyond what is expected within the constructor is
   *  done in here.
   *
   *  Note, this routine overloaded.  Additionally, it can set an
   *  ErrorHandler event if it encounters an unrecoverable error.
   *
   *  @see ErrorHandler    
   */
  void Init(const uint16_t msg_id, const HTTPFraming& hdr);

  /** Setup or initialize a MsgHdr from data within a char buffer.
   *
   *  This routine first sees if either it has enough data to build a
   *  *static* framing header, e.g., when using struct-based framing
   *  type, or calls the InitFromBuf() member function from the
   *  class-based framing type.  In either case, if we were successful
   *  at building a *complete* framing header, we return TRUE and
   *  populate our HdrStorage with the correct framing object.
   *
   *  TODO(aka) This routine should return the amount of data from buf
   *  that was used to initialize our object, not simply a bool!
   *
   *  Note, This routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler Class
   *  @param buf is a character buffer
   *  @return a pointer to any remaining data in the stream
   */
  size_t InitFromBuf(const char* buf, const size_t len);

  // Boolean checks.

  /** Routine to see if a message is a REQUEST (as opposed to a RESPONSE).
   *
   */
  bool IsMsgRequest(void) const;

  /** Routine to check for normal status on a message.
   *
   */
  bool IsMsgStatusNormal(void) const;

  // Flags.
  enum { TYPE_NONE, TYPE_BASIC, TYPE_HTTP };

 protected:
  // Data members.
  uint16_t msg_id_;             // unique message id

  // XXX Put this in HdrStorage!
  uint8_t type_;                // type of framing header we are

  HdrStorage hdr_;              // our *possible* headers

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.
  int operator ==(const MsgHdr& other) const;
};


#endif  /* #ifndef MSGHDR_H_ */

