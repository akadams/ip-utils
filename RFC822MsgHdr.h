/* $Id: RFC822MsgHdr.h,v 1.1 2012/02/03 13:17:10 akadams Exp $ */

// RFC822MsgHdr Class: library for controlling message-headers in
// RFC822 format.

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef RFC822MSGHDR_H_
#define RFC822MSGHDR_H_

#include <string>
#include <vector>
using namespace std;


// Message-header storage adhereing to RFC822.
struct rfc822_msg_hdr {
  string field_name;
  string field_value;  // field-body in rfc822

  // A field-body may also contain additional parameters, if so, these
  // will be in the vector below.

  vector<struct rfc822_parameter> parameters;
};

struct rfc822_parameter {
  string key;
  string value;
};

// Non-class specific utilities.


#endif  /* #ifndef RFC822MSGHDR_H_ */

