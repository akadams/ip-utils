// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef SOAPFRAMING_H_
#define SOAPFRAMING_H_

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

/**
 * Class for SOAP encapsulation.
 * Version: $Id: SOAPFraming.h,v 1.1 2012/02/03 13:17:10 akadams Exp $.
 */
class SOAPFraming {
 public:
  /** Constructor.
   *
   */
  SOAPFraming(void);

  /** Constructor.
   *  @param i an integer argument.
   */
  explicit SOAPFraming(const int i);

  /** Destructor.
   *
   */
  virtual ~SOAPFraming(void);

  // Copy constructor, assignment and equality operator needed for STL.

  // Accessors.

  // XXX Getter (accessors) & setter (mutators) methods do not need
  // Doxygen comments, unless something non=intuitive is going on.

  // XXX Place a space between accessors that are actual data members,
  // and those accessors that are derived from those data memebers,
  // e.g., num_polygons() is dervived from polygons_ in Polygon.

  // Mutators.
  void clear(void);

  // XXX Place a space between mutators that are actual data members,
  // and those mutators that operate on different objects to generate
  // the data members.

  /** Parse (or convert) a char* into a SOAPFraming object.
   *
   *  This routine takes a pointer to a character stream and attempts
   *  to parse the stream into a SOAPFraming object.  This routine will
   *  set an ErrorHandler event if it encounters an unrecoverable
   *  error.
   *
   *  @param stream is a character stream.
   *  @see SOAPFraming()
   *  @see ErrorHandler Class
   *  @return a pointer to any remaining data in the stream
   */
  //const char* Convert(const char* stream);

  // SOAPFraming manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** Routine to *print-out* the SOAP headers.
   *
   *  This routine uses the SOAPFraming object to write to a string
   *  the SOAP headers (which should be suitable to be passed to a
   *  network object (e.g., TCPSession).
   *
   *  @param offset an integer showing from *what point* of the header
   *  should be returned.
   *  @return a string is returned holding the message header
   */ 
  string print_hdr(const int offset) const;

  /** Routine to copy (or clone) a SOAPFraming object.
   *
   * As copy constructors are usually frowned apon (except when needed
   * for the STL), a Clone() method is provided.
   *
   *  @param src the source SOAPFraming to build our object from.
   */
  //void Clone(const SOAPFraming& src);

  /** Routine to initialize a SOAPFraming object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed.  This routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   */
  //void Init(void);

  // Boolean checks.

  // Flags.

 protected:
  // Data members.
  // int foo_;  ///< foo holder

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.

  /** Copy constructor.
   *
   */
  SOAPFraming(const SOAPFraming& src);

  /** Assignment operator.
   *
   */
  SOAPFraming& operator =(const SOAPFraming& src);

  /** Equality operator.
   */
  int operator ==(const SOAPFraming& other) const;
};


#endif  /* #ifndef SOAPFRAMING_H_ */

