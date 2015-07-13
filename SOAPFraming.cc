/* $Id: SOAPFraming.cc,v 1.2 2012/05/10 17:50:47 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

// XXX Any includes not in SOAPFraming.h in /usr/include/sys first (in alphabetical order), e.g.: #include <sys/types.h>

// XXX a space, then any network includes not in SOAPFraming.h next (in alphabetical order), e.g.: #include <netinet/tcp.h>

// XXX a space, then includes in /usr/include not in SOAPFraming.h (in alphabetical order), e.g. : #include <err.h>\n#include <stdint.h>

// XXX a space, STL includes not in SOAPFraming.h next.
#include <string>
using namespace std;

// XXX A space, and any local header files includes needed that are not in SOAPFraming.h.

#include "SOAPFraming.h"

// Non-class specific defines & data structures.

// Non-class specific utility functions.

// SOAPFraming Class.

// Constructors and destructor.
SOAPFraming::SOAPFraming(void) {
}

SOAPFraming::~SOAPFraming(void) {
}

// Copy constructor, assignment and equality operator, needed for STL.

// Accessors.

// Mutators.

// SOAPFraming manipulation.
SOAPFraming::print_hdr(const int offset) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  /*
<?xml version='1.0' ?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/">
  <SOAP-ENV:Body>
    ..
    <theSignedForm href="cid:claim.tiff@claiming-it.com"/>
    ..
  </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
  */

  // XXX TOOD(aka) Use Xerces-c to build XML ...

  const char* line1 = "<?xml version='1.0' ?>";
  const char* line2 = "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">";
  const char* line3 = "<SOAP-ENV:Body>";
  const char* line4 = "<theSignedForm href=\"cid:claim.tiff@claiming-it.com\"/>";
  const char* line5 = "</SOAP-ENV:Body>";
  const char* line6 = "</SOAP-ENV:Envelope>";

  xxx;
  int n = snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, "%s\r\n%s\r\n",
                   print_status_line().c_str(), print_msg_hdrs().c_str());
  if (n >= SCRATCH_BUF_SIZE) {
    // TODO(aka) We need to resize tmp_str and try again!
    _LOGGER(LOG_WARN, "HTTPFraming::print_hdr(): TODO(aka) "
            "scratch buffer size is %ld, but snprintf() returned %ld.",
            SCRATCH_BUF_SIZE, n);
  }

  if ((size_t)offset <= tmp_str.size())
    return tmp_str.substr(offset);  // just return what we want
  else 
    return tmp_str;
}

// Boolean checks.


// XXX Two spaces between classes within .cc file.
