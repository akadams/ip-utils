// Copyright © 2009, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef ERRORHANDLER_H_
#define ERRORHANDLER_H_

#include <sys/types.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#include <list>
#include <string>  
using namespace std;


#define ERRORHANDLER_EVENT_NULL 0
#define ERRORHANDLER_ERRNO_NULL 0

/** Class for managing and handling errors.
 *
 *  The ErrorHandler class allows the programmer to capture errors and
 *  store them for later processing, e.g., by the main event-loop.  It
 *  uses the same error realm codes as sysexits(3).  By including this
 *  header file, one can access the global ErrorHandler error class
 *  intance.
 *
 *  RCSID: $Id: ErrorHandler.h,v 1.1 2012/02/03 13:17:09 akadams Exp $
 *
 *  @author Andrew K. Adams <akadams@psc.edu>
 */
class ErrorHandler {
 public:
  /** Constructor.
   *
   */
  ErrorHandler(void);

  // Using implicit destructor (because, we should never need it).

  // Accessors.

  /** Routine to return the current system errno set in the ErrorHandler.
   *
   */
  int sys_errno(void) const { return sys_errno_; }

  /** Routine to print-out the error message.
   *
   *  @return a string holding the error message
   */
  string print(void) const;

  // Mutators.

  /** Routine to set the ErrorHandler's error realm.
   *
   *  Note, the values are those used in sysexits(3).
   */
  void set_realm(const int realm);

  /** Routine to set the system errno.
   *
   *  @param sys_errno an int representing the system errno for the error
   */
  void set_sys_errno(const int sys_errno);

  /** Routine to clear all error information from the global ErrorHandler.
   *
   */
  void clear(void);

  // Error manipulation.

  /** Routine to initialize an ErrorHandler event.
   *
   *  This routine populates (or intializes) our global ErrorHandler
   *  intance with the information provided as parameters to this
   *  function.
   *
   *  TODO(aka) This routine should be called Init().
   *
   *  @param realm an int specifying the sysexits(3) code
   *  @param format a varags format and variables specifying the message
   */
  void Init(const int realm, const char* format, ...);

  /** Routine to append a message to the current error event.
   *
   *
   *  @param format a varags format and variables specifying the message
   */
  void AppendMsg(const char* format, ...);

  // Boolean checks.

  /** Routine to see if an ErrorHandler event has been set.
   *
   *  @return a bool signifying whether or not an error was recorded
   */
  const bool Event(void) const { return event_flag_; }

  // Flags.

 protected:
  // Data members.
  bool event_flag_;	// flag to signal an error event occured
  int realm_;	        // realm of error (see sysexits(3))
  int sys_errno_;	// system errno, if applicable

  // TODO(aka) If we are concerned with memory, we might want to have
  // *one* static buffer ... just have to synchronize its usage.

  list<string> msgs_;	// list of messages, appened to by routines

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.
  ErrorHandler(const ErrorHandler& src);
  ErrorHandler& operator =(const ErrorHandler& src);
  int operator ==(const ErrorHandler& other);
};

extern ::ErrorHandler error;  // static declaration for users of this Class

	
#endif  /* #ifndef ERRORHANDLER_H_ */

