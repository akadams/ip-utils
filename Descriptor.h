// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _DESCRIPTOR_H_
#define _DESCRIPTOR_H_

#include <stdio.h>

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.
#define DESCRIPTOR_NULL -1

// Non-class specific utilities.


/**
 *  Class for controlling (file) descriptor reference counts.
 *
 * This class provides a mechanism to track the number of refernces to
 * a file descriptor, such that, prior to destructing this class (in
 * the parent class), the reference count can be checked to see if
 * it's safe to close(2) (or fclose(3)) the descriptor (and also,
 * delete this object).
 *
 * This class is *NOT* to be used as a base class (@see RefCount for
 * derviving classes off of a reference counting object), as we can
 * not implement a copy constructor (simply because we can't change
 * the ref count in the const src parameter), which is required for
 * STL.
 *
 * Thus, the parent class is responsible for incrementing and
 * decrementing the ref count in its destructor, copy constructor and
 * assignment operator.  If the ref count is 0, the parent is then
 * also responsible for destructing the Descriptor class (after
 * closing the descriptor!).  I repeat, it's the onus of the parent to
 * INCREMENT & DECREMENT Discriptor's ref count in the parent's copy
 * constructor, assignment operator and destructor, and then see if
 * the Descriptor object should be deleted!
 *
 * Notes:
 *
 * - If the descriptor is opened for streaming I/O, then fd_ remains
 * DESCRIPTOR_NULL and we use fp_.
 *
 * - We could either make the data members public in here, or we need
 * to declare the parent classes as friends to this class.  We chose
 * the latter, for the number of classes that would want to use this
 * class are finite and tractable.
 *
 * RCSID: $Id: Descriptor.h,v 1.1 2012/02/03 13:17:09 akadams Exp $.
 *
 * @author Andrew K. Adams <akadams@psc.edu>
 */
class Descriptor {
 public:
  /** Constructor.
   *
   */
  Descriptor(void);

  /** Destructor.
   *
   */
  virtual ~Descriptor(void);

  // Accessors.

  /** Routine to return the file descriptor.
   *
   *  @return an int specifying the file descriptor
   */
  int fd(void) const { return fd_; }

  /** Routine to return the streaming FILE pointer.
   *
   *  @return a reference to the FILE pointer
   */
  const FILE* fp(void) const { return fp_; }

  // Mutators.

  // Note, all work on the data members will be done in the friend
  // classes.

  // Descriptor manipulation.

  // Boolean checks.

  // Flags.

  friend class File;
  friend class IPComm;
  friend class TCPConn;
  friend class UDPConn;
  friend class SSLConn;

 protected:
  // Data members.
  int fd_;        // the file descriptor
  FILE* fp_;      // the file descriptor as a stream (currently only
                  // used in File Class)

  unsigned int cnt_;   // reference count for this descriptor

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.

  /** Copy Constructor.
   *
   *  The copy constructor is needed for class objects used within the STL.
   */
  Descriptor(const Descriptor& src);  // TODO(aka) no way to update ref count in src!

  /** Assignment Operator.
   *
   *  The assignment operator is needed for class objects used within the STL.
   */
  Descriptor& operator =(const Descriptor& src);

  int operator ==(const Descriptor& other) const;
};


#endif  /* #ifndef _DESCRIPTOR_H_ */

