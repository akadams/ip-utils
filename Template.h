// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef TEMPLATE_H_
#define TEMPLATE_H_

// XXX includes in /usr/include/sys first (in alphabetical order), e.g.: #include <sys/types.h>

// XXX a space, then any network includes next (in alphabetical order), e.g.: #include <netinet/tcp.h>

// XXX a space, then includes in /usr/include (in alphabetical order), e.g. : #include <err.h>\n#include <stdint.h>

// XXX a space, STL includes next.
#include <string>
using namespace std;

// XXX A space, and any local header files includes needed.

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.

// Non-class specific utilities.

// XXX Note, all comments that start with "/**" are for Doxygen.

/** Class for controlling templates.
 *
 *  A more detailed decription of the template.
 *
 *  RCSID: $Id: Template.h,v 1.2 2012/05/22 16:33:27 akadams Exp $.
 *
 *  @author Andrew K. Adams <akadams@psc.edu>
 */
class Template {
 public:
  /** Constructor.
   *
   */
  Template(void);

  /** Constructor.
   *  @param i an integer argument.
   */
  explicit Template(const int i);

  /** Destructor.
   *
   */
  virtual ~Template(void);

  // Copy constructor, assignment and equality operator needed for STL.

  // Accessors.

  // XXX Getter (accessors) & setter (mutators) methods do not need
  // Doxygen comments, unless something non=intuitive is going on.

  // XXX Place a space between accessors that are actual data members,
  // and those accessors that are *derived from* our data memebers,
  // e.g., num_foo() may be derived from "vector<int> foo_".

  // Mutators.
  void clear(void);

  // XXX Place a space between mutators that are actual data members,
  // and those mutators that operate on different objects to
  // manipulate our data members.

  // Template manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** Routine to copy (or clone) a Template object.
   *
   * As copy constructors are usually frowned apon (except when needed
   * for the STL), a Clone() method is provided.
   *
   *  @param src the source Template to build our object from.
   */
  void Clone(const Template& src);

  /** Routine to initialize a Template object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed.  This routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   */
  void Init(void);

  /** Parse (or convert) a char* into a Template object.
   *
   *  This routine takes a pointer to a character stream and attempts
   *  to parse the stream into a Template object.  This routine will
   *  set an ErrorHandler event if it encounters an unrecoverable
   *  error.
   *
   *  @see ErrorHandler Class
   *  @param buf is a char* stream
   *  @param len is a size_t representing the size of buf
   *  @return a size_t showing how much data from buf we used
   */
  size_t InitFromBuf(const char* buf, const size_t len);

  // Boolean checks.

  // Flags.

 protected:
  // Data members.
  int foo_;  ///< foo holder

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.

  /** Copy constructor.
   *
   */
  Template(const Template& src);

  /** Assignment operator.
   *
   */
  Template& operator =(const Template& src);

  /** Equality operator.
   */
  int operator ==(const Template& other) const;
};


#endif  /* #ifndef TEMPLATE_H_ */

