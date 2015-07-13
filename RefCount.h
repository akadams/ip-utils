// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _REFCOUNT_H_
#define _REFCOUNT_H_

#include <stdio.h>

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.

// Non-class specific utilities.


/**
 *  Class for adding reference counts.
 *
 *
 * RCSID: $Id: RefCount.h,v 1.1 2012/02/03 13:17:09 akadams Exp $.
 *
 * @author Andrew K. Adams <akadams@psc.edu>
 */
class RefCount {
 public:
  /** Constructor.
   *
   */
  RefCount(void);

  /** Destructor.
   *
   */
  virtual ~RefCount(void);

  /** Copy Constructor.
   *
   *  The copy constructor is needed for class objects used within the STL.
   */
  RefCount(const RefCount& src);

  /** Assignment Operator.
   *
   *  The assignment operator is needed for class objects used within the STL.
   */
  RefCount& operator =(const RefCount& src);

  // Accessors.

  // Mutators.

  // Note, all work on the data members will be done in the friend
  // classes.

  // RefCount manipulation.

  // Boolean checks.

  // Flags.

  friend class Foo;

 protected:
  // Data members.
  int cnt_;       // reference count for this descriptor

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.
  int operator ==(const RefCount& other) const;
};


#endif  /* #ifndef _REFCOUNT_H_ */

