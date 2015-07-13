/* $Id: Descriptor.cc,v 1.1 2012/02/03 13:17:09 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <err.h>

#include "Descriptor.h"

#define DEBUG_CLASS 0

// Non-class specific defines & data structures.

// Non-class specific utilities.

// Descriptor Class.

// Constructors and destructor.
Descriptor::Descriptor(void) { 
#if DEBUG_CLASS
  warnx("Descriptor::Descriptor(void) called.");
#endif

  fd_ = DESCRIPTOR_NULL; 
  fp_ = NULL;
  cnt_ = 1;
}

Descriptor::~Descriptor(void) {
#if DEBUG_CLASS
  warnx("Descriptor::~Descriptor(void) called.");
#endif
  // Remember, decrements are done in the parent, so the parent knows
  // when to close the file descriptor, e.g., IPComm::Close() or
  // File::Close(), prior to calling ~Descriptor().
}

// Copy constructor, assignment and equality operator, needed for STL.
Descriptor::Descriptor(const Descriptor& src) {
  fd_ = src.fd_;
  fp_ = src.fp_;
  cnt_ = src.cnt_;
}

// Accessors.

// Mutators.

// Template manipulation.

// Boolean checks.

// Flags.
