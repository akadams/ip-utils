/* $Id: MIMEFraming.h,v 1.2 2013/02/11 15:48:31 akadams Exp $ */

// MIMEFraming Class: library for controlling mesasge framing using MIME.

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef MIMEFRAMING_H_
#define MIMEFRAMING_H_

#include <string>
#include <vector>
#include <map>
using namespace std;

#include "RFC822MsgHdr.h"

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.

#define MIMEFRAMING_MAX_HDR_SIZE 1024              // max MIME hdr (minus data)
#define MIMEFRAMING_MAX_MULTIPART_SIZE (1024 * 8)  // max mulitpart data size 

#define MIME_VERSION_MAJOR 1
#define MIME_VERSION_MINOR 0

// Message-header field types.
#define MIME_VERSION "MIME-Version"
#define MIME_CONTENT_LENGTH "Content-Length"
#define MIME_CONTENT_ENCODING "Content-Encoding"
#define MIME_CONTENT_TYPE "Content-Type"

// Message-header "Content-Type" field values and parameters.

// TOOD(aka) Can't we get these from a header file *somewhere* on the system?
#define MIME_TEXT_HTML "text/html"
#define MIME_TEXT_XML "text/xml"
#define MIME_TEXT_PLAIN "text/plain"

#define MIME_APP_JSON "application/json"

#define MIME_IMAGE_GIF "image/gif"
#define MIME_IMAGE_PNG "image/png"

#define MIME_VIDEO_MPEG "video/mpeg"
#define MIME_VIDEO_MP4 "video/mp4"
#define MIME_VIDEO_QUICKTIME "video/quicktime"
#define MIME_VIDEO_OGG "video/ogg"

// TODO(aka) RFC's 2045 and 2046 suggest something like this for .tgz
// / .tar.gz
//
//  Content-Type: application/x-tar; encoding="gzip"

#define MIME_APP_TAR "application/x-tar"
#define MIME_APP_GZIP "application/x-gzip"
#define MIME_APP_OCT_STREAM "application/octet-stream"
#define MIME_BINARY MIME_APP_OCT_STREAM
#define MIME_CHARSET "charset"
#define MIME_ISO_8859_1 "ISO-8859-1"
#define MIME_CONTENT_TYPE_TEXT "text/"  // TOOD(aka) used to add charset

// Non-class specific utilities.

// Class description.
class MIMEFraming {
 public:
  // Constructors and destructor.
  MIMEFraming(void);
  explicit MIMEFraming(const int i);

  virtual ~MIMEFraming(void);

  // Copy constructor, assignment and equality operator needed for STL.

  // Accessors.

  // Mutators.
  void clear(void);

  // const char* Convert(const char* buf);
  void add_multipart();

  // MIMEFraming manipulation.
  // string print(void) const;

  // XXX TOOD(aka) We need a separate print_multipart(i, offset)
  // routine that will print out just a multipart section (and an
  // offset into that section (as well as a length to use).

  string print_hdr(const int offset) const;
  // void Clone(const MIMEFraming& src);
  // void Init(void);  // ErrorHandler

  // Boolean checks.

  // Flags.

 protected:
  // Data members.
  vector<struct rfc822_msg_hdr> msg_hdrs_;          // MIME message headers
  map<int, vector<struct rfc822_msg_hdr> > multipart_hdrs_;  // content-headers

  // XXX TOOD(aka) We need a union of either a string or a File.
  map<int, string> multipart_data_;                          // content data

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.
  MIMEFraming(const MIMEFraming& src);
  MIMEFraming& operator =(const MIMEFraming& src);
  int operator ==(const MIMEFraming& other) const;
};


#endif  /* #ifndef MIMEFRAMING_H_ */

