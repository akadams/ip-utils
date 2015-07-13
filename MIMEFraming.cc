/* $Id: MIMEFraming.cc,v 1.4 2013/09/13 14:56:38 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <err.h>

#include "ErrorHandler.h"
#include "Logger.h"
#include "MIMEFraming.h"

// Non-class specific defines & data structures.
#define SCRATCH_BUF_SIZE (MIMEFRAMING_MAX_HDR_SIZE + \
                          MIMEFRAMING_MAX_MULTIPART_SIZE)

const char* kNonMIMECompliantClientMsg = 
    "This is a message with multiple parts in MIME format.";
const char* kMIMEBoundary = "MIME_Boundary";

// Non-class specific utility functions.

// MIMEFraming Class.

// Constructor.
MIMEFraming::MIMEFraming(void) {
  // Add MIME version message-header.
  struct rfc822_msg_hdr tmp_msg_hdr;
  tmp_msg_hdr.field_name = MIME_VERSION;
  string tmp_str(16, '\0');  // '\0' so strlen() works
  snprintf((char*)tmp_str.c_str(), 16, "%d.%d", 
           MIME_VERSION_MAJOR, MIME_VERSION_MINOR);
  tmp_msg_hdr.field_value = tmp_str;
  msg_hdrs_.push_back(tmp_msg_hdr);

}

// Destructor.
MIMEFraming::~MIMEFraming(void) {
  // For now, nothing to do.
}

// Copy constructor, assignment and equality operator, needed for STL.

// Accessors.

// Mutators.

// MIMEFraming manipulation.

// Routine to print-out the MIME message header.
string MIMEFraming::print_hdr(const int offset) const {
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  /*  TODO(aka) Sample MIME header.
MIME-Version: 1.0
Content-Type: Multipart/Related; boundary=MIME_boundary; type=text/xml;
        start="<claim.xml@claiming-it.com>"
Content-Description: This is the optional message description.

--MIME_boundary
Content-Type: text/xml; charset=UTF-8
Content-Transfer-Encoding: 8bit
Content-ID: <claim.xml@claiming-it.com>

<?xml version='1.0' ?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/">
  <SOAP-ENV:Body>
    ..
    <theSignedForm href="cid:claim.tiff@claiming-it.com"/>
    ..
  </SOAP-ENV:Body>
</SOAP-ENV:Envelope>

--MIME_boundary
Content-Type: image/tiff
Content-Transfer-Encoding: binary
Content-ID: <claim.tiff@claiming-it.com>

d3d3Lm1hcmNoYWwuY29taesgfSEVFES45345sdvgfszd==
--MIME_boundary--
  */

  // First, print out the MIME Content-headers.
  int n = 0;
  for (int i = 0; i < (int)msg_hdrs_.size(); i++) {
    if (msg_hdrs_[i].parameters.size()) {
      n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                    SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "%s: %s; ", 
                    msg_hdrs_[i].field_name.c_str(), 
                    msg_hdrs_[i].field_value.c_str());

      vector<struct rfc822_parameter>::const_iterator param = 
          msg_hdrs_[i].parameters.begin();
      while (param != msg_hdrs_[i].parameters.end()) {
        n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                      SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "%s=%s", 
                      param->key.c_str(), param->value.c_str());

        param++;
        if (param != msg_hdrs_[i].parameters.end())
          n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                        SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "; ");
        else
          n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                        SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "\r\n");
      }
    } else {
      n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                    SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "%s: %s\r\n", 
                    msg_hdrs_[i].field_name.c_str(), 
                    msg_hdrs_[i].field_value.c_str());
    }
  }  // for (int i = 0; i < (int)msg_hdrs_.size(); i++) {

  // TODO(aka) I guess we could send a non-multipart message, but
  // for now, let's assume all are multipart.

  // After the MIME message headers, add some text for clients that
  // don't support multipart messages.

  n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "\r\n%s\r\n", 
                kNonMIMECompliantClientMsg);

  // Now add each multipart message.
  map<int, vector<rfc822_msg_hdr> >::const_iterator multipart =
      multipart_hdrs_.begin();
  while (multipart != multipart_hdrs_.end()) {
    // Add the start-boundary.
    n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                  SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "--%s\r\n", 
                  kMIMEBoundary);

    int key = multipart->first;  // get the associative key

    // Print out each content-header for the multipart message.
    for (int j = 0; j < (int)multipart->second.size(); j++) {
      if (multipart->second[j].parameters.size()) {
        n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                      SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "%s: %s; ", 
                      multipart->second[j].field_name.c_str(), 
                      multipart->second[j].field_value.c_str());

        vector<struct rfc822_parameter>::const_iterator param = 
            multipart->second[j].parameters.begin();
        while (param != multipart->second[j].parameters.end()) {
          n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                        SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "%s=%s", 
                        param->key.c_str(), param->value.c_str());

          param++;
          if (param != multipart->second[j].parameters.end())
            n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                          SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "; ");
          else
            n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                          SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), "\r\n");
        }
      } else {
        n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                      SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "%s: %s\r\n", 
                      multipart->second[j].field_name.c_str(), 
                      multipart->second[j].field_value.c_str());
      }
    }  // for (int j = 0; j < (int)multipart->second.size(); j++) {

    // Add the multipart's data.
    map<int, string>::const_iterator data = multipart_data_.begin();
    while (data != multipart_data_.end()) {
      if (data->first == key) {
        n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                      SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),
                      "\r\n%s\r\n", data->second.c_str());
        break;
      }

      data++;
    }
    if (data == multipart_data_.end()) {
      _LOGGER(LOG_ERR, "MIMEFraming::print_hdr(): "
           "multipart(%d) data not found!", key);
      return tmp_str;  // TODO(aka) return what we have so far
    }

    // Add the closing boundary; if we are the last, be sure to
    // complete it with two hyphens "--".

    multipart++;
    if (multipart != multipart_hdrs_.end())
      n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                    SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "--%s\r\n", 
                    kMIMEBoundary);

    else 
      n += snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                    SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()),  "--%s--\r\n", 
                    kMIMEBoundary);
  }  // for (int i = 0; i < (int)multipart_hdrs_.size(); i++) {

  if (n >= SCRATCH_BUF_SIZE) {
    // TODO(aka) We need to resize tmp_str and try again!
    _LOGGER(LOG_WARNING, "HTTPFraming::print_hdr(): "
            "scratch buffer size is %ld, but snprintf() returned %ld.",
            SCRATCH_BUF_SIZE, n);
    // Fall-through to return what we have so far.
  }

  if ((size_t)offset <= tmp_str.size())
    return tmp_str.substr(offset);  // just return what we want
  else 
    return tmp_str;
}

// Boolean checks.

