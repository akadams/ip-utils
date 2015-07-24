// Copyright © 2009, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef URL_H_
#define URL_H_

#include <netinet/in.h>

#include <sys/types.h>

#include <string>
#include <list>
using namespace std;

#include "ErrorHandler.h"

#define URL_XML_ELEMENT "url"
#define URL_VERSION_MAJOR 1
#define URL_VERSION_MINOR 0

#define URL_PORT_NULL 0
#define URL_FILE_EXT ".url"

#define URL_NAT_PROXY "NAT-PROXY"

const size_t kURLMaxSize = (1024 * 4);

// Non-class specific defines & data structures.
struct url_query_info {
  string key;
  string value;
};

// Non-class specific utilities.

/** Routine to see if a char string contains any *nasty* characters.
 *
 */
bool is_str_tainted(const char* str);

/**
 * Class for creating, manipulating and managing URLs.
 *
 * RCSID: $Id: URL.h,v 1.1 2012/02/03 13:17:10 akadams Exp $
 *
 * @author Andrew K. Adams <akdams@psc.edu>
 */
class URL {
 public:
  /** Constructors.
   *
   */
  URL(void);

  /** Destructor.
   *
   */
  virtual ~URL(void);

  /** Copy constructor, needed for use with STL.
   *
   */
  URL(const URL& src);

  /* Assignment operator, needed for use with STL.
   *
   */
  URL& operator =(const URL& src);

  // Accessors.
  string scheme(void) const { return scheme_; }
  string host(void) const { return host_; }
  in_port_t port(void) const { return port_; }
  string path(void) const { return path_; }
  list<struct url_query_info> query(void) const { return query_; }
  string fragment(void) const { return fragment_; }

  // Mutators.
  void set_scheme(const char* scheme);
  void set_host(const char* host);
  void set_port(const in_port_t port);
  void set_path(const char* path);
  void set_query(const char* query_buf, const size_t len);
  void set_fragment(const char* fragment);
  void clear(void);

  // URL manipulation.

  /** Routine to *print-out* an URL object.
   *
   *  Depending on the scheme and port, you will either get just a
   *  hostname, a 2-tuple, i.e., hostname COLON port, or a complete
   *  URL.
   *
   */ 
  string print(void) const;

  //const char* print_xml(const int indent_level, const char* element = URL_XML_ELEMENT, const char* version_attribute = NULL) const;

  /** Routine to create (or initialize) an URL object from its components.
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   */
  void Init(const char* scheme, const char* host, const in_port_t port, 
            const char* query, const size_t len, const char* fragment);

  /** Routine to create (or initialize) an URL object from a char buffer.
   *
   *  InitFromBuf() attempts to initialize an URL object from the data
   *  within a char buffer.
   *
   *  This routine will set an ErrorHandler event if it encounters an
   *  unrecoverable error.
   *
   *  @see ErrorHandler Class
   *  @param buf a char* stream
   *  @param len a size_t specifying the size of buf
   *  @return a size_t showing how much data from buf was used (or 0)
   */
  size_t InitFromBuf(const char* buf, const size_t len, 
                     const in_port_t default_port);

  /** Routine to compare two URLs.
   *
   *  CompareTuples specifically compares the IP tuples, i.e., just
   *  the host & port, within the URLs.  Additionally, the flag
   *  exact_port can specify whether or not the port should be used in
   *  the comparision!
   *
   */
  int CompareTuples(const URL& other, const int exact_port) const;
	
  // Boolean checks.

  // Flags.
  enum { IGNORE_PORT, MATCH_PORT };

  // Friend functions & classes.
  friend int compare_tuples(const URL& right, const URL& left);

 protected:
  // Data members.
  string scheme_;
  string host_;
  in_port_t port_;
  string path_;
  list<struct url_query_info> query_;    // we can have multiple key-value pairs in a query
  string fragment_;

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.
  int operator ==(const URL& other) const;
};


#endif  /* #ifndef URL_H_ */
