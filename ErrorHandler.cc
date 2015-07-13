/* $Id: ErrorHandler.cc,v 1.2 2012/04/19 14:00:10 akadams Exp $ */

// Copyright (c) 2010, see the file 'COPYRIGHT.txt' for any restrictions.

#include <err.h>

#include "ErrorHandler.h"


#define SCRATCH_BUF_SIZE (1024 * 4)

static const char* null_msg = "Internal error: msg_ is NULL";

ErrorHandler error;	// global definition, for users of this Class


// Non-class specific utility functions.

// Constructor and destructor functions.
ErrorHandler::ErrorHandler(void) {
  event_flag_ = false;
  realm_ = EX_OK;
  sys_errno_ = ERRORHANDLER_ERRNO_NULL;
}

// Class manipulation functions.

// Accessors.

// Routine to *pretty* print object.
string ErrorHandler::print(void) const {
  // Set aside space for returned message.
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  // Walk list, and append each message into our buffer ...
  list<string>::const_iterator p = msgs_.begin();
  while (p != msgs_.end()) {
    snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
             SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), 
             "%s", (*p).c_str());

    p++;

    if (p != msgs_.end()) {
      // Add a ", ", if we aren't done with our list.
      snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
               SCRATCH_BUF_SIZE - strlen(tmp_str.c_str()), ": ");
    } 
  }

  return tmp_str;
}

// Mutators.
void ErrorHandler::set_realm(const int realm) {
  realm_ = realm;

  return;
}

void ErrorHandler::set_sys_errno(const int sys_errno) {
  sys_errno_ = sys_errno;

  return;
}

void ErrorHandler::Init(const int realm, const char* format, ...) {
  // Set aside space for ERROR message.
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  if (event_flag_) {
    // Hmm, someone already called Init ... don't overwrite realm
    // and insert a warning in msg_.

    msgs_.push_back("WARN: ErrorHandler::Init() event_flag alrady set: ");
  } else {
    set_realm(realm);
  }

  if (format && strlen(format)) {
    // Add the variable length stuff.
    va_list ap;
    va_start(ap, format);
    vsnprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, format, ap);
    va_end(ap);
  } else {
    strncpy((char*)tmp_str.c_str(), null_msg, SCRATCH_BUF_SIZE - 1);
  }

  msgs_.push_back(tmp_str);

  event_flag_ = true;  // mark that an event occurred

  return;
}

void ErrorHandler::AppendMsg(const char* format, ...) {
  // Set aside space for appended message.
  string tmp_str(SCRATCH_BUF_SIZE, '\0');  // '\0' so strlen() works

  // Sanity check that ErrorHandler::Report() was called first!
  if (!event_flag_) {
    // Technically, this should be an error, but let's deal.
    msgs_.push_back("WARN: ErrorHandler::append_msg(): event_flag not set: ");
    set_realm(EX_SOFTWARE);
    event_flag_ = true;  // mark that an event occurred
  }

  if (format && strlen(format)) {
    // Add the variable length stuff.
    va_list ap;
    va_start(ap, format);
    vsnprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, format, ap);
    va_end(ap);
  } else {
    strncpy((char*)tmp_str.c_str(), null_msg, SCRATCH_BUF_SIZE - 1);
  }

  msgs_.push_back(tmp_str);

  return;
}

void ErrorHandler::clear(void) {
  event_flag_ = false;
  realm_ = EX_OK;
  sys_errno_ = ERRORHANDLER_ERRNO_NULL;
  msgs_.clear();
}

// Error manipulation routines.

#if 0
string ErrorHandler::print_debug(void) const
{
	// A more 'detailed' Print(), probably only used for debugging ...

	static char scratch_buffer[SCRATCH_BUF_SIZE];

	snprintf((char*)tmp_str.c_str(), SCRATCH_BUF_SIZE, 
		 "%d:%d:%d %s", event, realm, sys_errno, msgs.Print());

	return tmp_str;
}
#endif
