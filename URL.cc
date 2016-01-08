/* $Id: URL.cc,v 1.5 2012/05/22 16:33:27 akadams Exp $ */

// Copyright (c) 2010, see the file "COPYRIGHT.h" for any restrictions.

#if defined(__linux__)
#include <linux/limits.h>	// for PATH_MAX
#else
#include <sys/syslimits.h>	// for PATH_MAX
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Logger.h"
#include "TCPConn.h"

#include "URL.h"

#define DEBUG_PARSE 0


// RFC 3986 query delimiters.
static const char kQueryStart = '?';
static const char kKeyValueDelimiter = '=';
static const char kQueryDelimiter = '&';
static const char kFragmentStart = '#';

// static URL* null_url_non_const = NULL;	// leave in to remember how

const char* kStringTainted = "STR_TAINTED";

// Non-class specific utility functions.

// Routine to see if a char string contains any *nasty* characters.
bool is_str_tainted(const char* str) {
  if (str == NULL)
    return false;

  for (int i = 0; str[i] != '\0'; i++) {
    // Check to see if we have any unpure characters.
    if (str[i] == '`' || 
        str[i] == '|' || 
        str[i] == '\\')
      return true;
  }

  return false;  // we made it here, so the string's probably not tainted
}

// Friend utility functions.
int compare_tuples(const URL& right, const URL& left) {
  return right.CompareTuples(left, URL::IGNORE_PORT);
}

// Constructors & destructors functions.
URL::URL(void) {
  port_ = URL_PORT_NULL;
}

URL::~URL(void) {
  // Nothing to do ...
}

// Copy constructor, assignment and equality operator needed for STL.
URL::URL(const URL& src) 
    : scheme_(src.scheme_), host_(src.host_), path_(src.path_), 
      query_(src.query_), fragment_(src.fragment_) {
  port_ = src.port_;
}

URL& URL::operator =(const URL& src) {
  scheme_ = src.scheme_;
  host_ = src.host_;
  port_ = src.port_;
  path_ = src.path_;
  query_ = src.query_;
  fragment_ = src.fragment_;

  return *this;
}

// Accessors & mutators.

void URL::set_scheme(const char* scheme) {
  if (scheme == NULL)
    return;

  if (!is_str_tainted(scheme))
    scheme_ = scheme;
  else
    scheme_ = kStringTainted;
}

void URL::set_host(const char* host) {
  if (host == NULL)
    return;

  if (!is_str_tainted(host))
    host_ = host;
  else
    host_ = kStringTainted;
}

void URL::set_port(const in_port_t port) {
  port_ = port;
}

void URL::set_path(const char* path_buf, const size_t len) {
  if (path_buf == NULL || len <= 0)
    return;

  if (is_str_tainted(path_buf)) {
    path_ = kStringTainted;
    return;
  }

  // Grab the path, character by character ...
  char tmp_buf[PATH_MAX + 1];
  char* buf_ptr = &tmp_buf[0];
  size_t i = 0;
  while (i++ < len && *path_buf != '\0' &&
         *path_buf != kQueryStart && *path_buf != kFragmentStart) {
    *buf_ptr++ = *path_buf++;
  }
  *buf_ptr = '\0';  // null-terminate tmp_buf
  path_ = tmp_buf;

  // Now, see if we had a query or a fragment ...
  if (*path_buf == kQueryStart)
    set_query(path_buf, len - i);
  else if (*path_buf == kFragmentStart)
    set_fragment(path_buf);  // set_fragment() does not use len yet
}

void URL::set_query(const char* query_buf, const size_t len) {
  if (query_buf == NULL)
    return;  // nothing to do

  // TODO(aka) Would be nice to look for fragments in here too!

  // TODO(aka) Change routine to use len, as opposed to relying on null
  // termination.

  if (query_buf[len] != '\0' || len > kURLMaxSize) {
    error.Init(EX_SOFTWARE, "URL::set_query(): "
               "buf is not null terminated or too large");
    return;
  }

  char scratch_buf[kURLMaxSize];

  const char* buf_ptr = query_buf;
  struct url_query_info tmp_info;

  size_t n = len;
  while (n > 0) {
    // Look for delimiter ...
    char* ptr = &scratch_buf[0];
    while (n > 0 && *buf_ptr != kKeyValueDelimiter && !isspace(*buf_ptr)) {
      *ptr++ = *buf_ptr++;
      n--;
    }

    if (*buf_ptr != kKeyValueDelimiter) {
      error.Init(EX_SOFTWARE, "URL::set_query(): "
                 "buf (%s) does not have a \'%c\'", 
                 query_buf, kKeyValueDelimiter);
      return;
    }

    *ptr = '\0';  // null terminate the key
    tmp_info.key = scratch_buf;

    buf_ptr++;  // move past key/value delimiter
    n--;
    
    // Find end of value.
    ptr = &scratch_buf[0];
    while (n > 0 && *buf_ptr != kQueryDelimiter && !isspace(*buf_ptr)) {
      *ptr++ = *buf_ptr++;
      n--;
    }

    *ptr = '\0';  // null terminate the value
    tmp_info.value = scratch_buf;
    query_.push_back(tmp_info);  // add new key/value pair to our list of queries

    if (*buf_ptr == kQueryDelimiter) {
      buf_ptr++;  // move past query delimiter
      n--;
    }
  }
}

void URL::set_fragment(const char* fragment) {
  if (fragment == NULL)
    return;

  // TODO(aka) Change routine to use len, as opposed to relying on null
  // termination.

  if (!is_str_tainted(fragment))
    fragment_ = fragment;
  else
    fragment_ = kStringTainted;
}

void URL::clear(void) {
  scheme_.clear();
  host_.clear();
  set_port(URL_PORT_NULL); 
  path_.clear();
  query_.clear();
  fragment_.clear();
}

// URL manipulation.

// Routine to *pretty* print out the URL object.
string URL::print(void) const {
  string tmp_str(kURLMaxSize, '\0');  // '\0' so strlen() works

  if (scheme_.size() > 0)
    snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
             kURLMaxSize - strlen(tmp_str.c_str()), "%s://", 
             scheme_.c_str());

  if (host_.size() > 0)
    snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
             kURLMaxSize - strlen(tmp_str.c_str()), "%s", 
             host_.c_str());

  if (port_ != URL_PORT_NULL)
    snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
             kURLMaxSize - strlen(tmp_str.c_str()), ":%hu", 
             port_);

  if (path_.size() > 0)
    snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
             kURLMaxSize - strlen(tmp_str.c_str()), "/%s", 
             path_.c_str());

  if (query_.size()) {
    snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
             kURLMaxSize - strlen(tmp_str.c_str()), "?");
    list<struct url_query_info>::const_iterator itr = query_.begin();
    while (itr != query_.end()) {
      snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
               kURLMaxSize - strlen(tmp_str.c_str()), "%s", 
               itr->key.c_str());

      snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
               kURLMaxSize - strlen(tmp_str.c_str()), "=%s", 
               itr->value.c_str());
      if (++itr != query_.end())
        snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
                 kURLMaxSize - strlen(tmp_str.c_str()), "&");
    }
  }

  if (fragment_.size() > 0)
    snprintf((char*)tmp_str.c_str() + strlen(tmp_str.c_str()), 
             kURLMaxSize - strlen(tmp_str.c_str()), "#%s", 
             fragment_.c_str());

  return tmp_str;
}

#if 0
// TODO(aka) We  probably will need this!
const char* URL::print_xmlt(const int indent_level, const char* element, 
                            const char* version_attribute) const {
  // Simple generic routine, that doesn't know much about XML!

  static char xml_scratch_buffer[kURLMaxSize];

  memset(xml_scratch_buffer, 0, kURLMaxSize);

  for (int i = 0; i < indent_level && i < kURLMaxSize; i++)
    strncat(xml_scratch_buffer, "\t", 1);

  if (version_attribute)
    snprintf(xml_scratch_buffer + strlen(xml_scratch_buffer), 
             kURLMaxSize - strlen(xml_scratch_buffer),
             "<%s %s = \'%d.%d\'>%s</%s>",
             element, version_attribute, 
             URL_VERSION_MAJOR, 
             URL_VERSION_MINOR,
             Print(), element);
  else
    snprintf(xml_scratch_buffer + strlen(xml_scratch_buffer), 
             kURLMaxSize - strlen(xml_scratch_buffer),
             "<%s>%s</%s>",
             element, Print(), element);

  return &xml_scratch_buffer[0];	// caution: static buffer
}
#endif

void URL::Init(const char* scheme, const char* host, const in_port_t port, 
               const char* path_buf, const size_t path_len,
               const char* query_buf, const size_t query_len,
               const char* fragment) {
  set_scheme(scheme);
  set_host(host);
  set_port(port);
  if (path_len)
    set_path(path_buf, path_len);
  if (query_len)
    set_query(query_buf, query_len);
  set_fragment(fragment);
  if (error.Event())
    error.AppendMsg("URL::Init(): ");
}

// Routine to populate a URL object using a char* ASCII stream.  That
// is, data will be consumed from the stream until a URL object is
// made.  The amount of data used from buf is returned.
size_t URL::InitFromBuf(const char* buf, const size_t len, 
                        const in_port_t default_port) {
  // Note, URLs can be a host or tuple, e.g.:
  //
  //	"foo.bar.edu", "foo:13500" 
  //
  // or from an actual URI, e.g.: 
  //
  //    scheme :// hierarchical-sequence [? query] [# fragment]
  //
  // where hier-seq is usally an authority SLASH path, e.g.:
  //
  //	http://blah:13500/fnord?who=lovecraft#arkham

  // UNTAINT TODO(aka) Need to make sure we have no unwanted characters!

  if (buf == NULL) {
    error.Init(EX_SOFTWARE, "URL::InitFromBuf(): buf is NULL");
    return 0;
  }

  clear();  // start from scratch

  if (strlen(buf) == 0)
    return 0;  // if no work, return

  char scratch_buffer[kURLMaxSize];
  size_t n = len;  // set aside byte cnt

#if DEBUG_PARSE
  _LOGGER(LOG_NORMAL, "URL::InitFromBuf(): buf[%ld]: %s, %hu.",
          (len - n), buf, default_port);
#endif

  const char* buf_ptr = buf;	// setup a walk pointer
  while (n > 0 && isspace(*buf_ptr)) {
    buf_ptr++;	// skip leading whitespace
    n--;
  }
  if (n == 0)
    return 0;  // not enough data

  // First, see if we have a scheme.
  const char* delimiter_ptr = NULL;
  if ((delimiter_ptr = strchr(buf_ptr, ':')) && *(delimiter_ptr + 1) == '/' && 
      *(delimiter_ptr + 2) == '/') {
    // We have a standard URL header, so get the scheme.
    char* ptr = &scratch_buffer[0];
    while (n > 0 && *buf_ptr != ':') {
      *ptr++ = *buf_ptr++;
      n--;
    }
    if (n == 0)
      return 0;  // not enough data
    *ptr = '\0';  // null terminate the scheme
    set_scheme(scratch_buffer);

    buf_ptr += 3;  // skip over "://" following scheme
    if (n < 4)  // account for "://" and at least a one char host
      return 0;  // not enough data
    n -= 3;
  }

#if DEBUG_PARSE
  _LOGGER(LOG_NORMAL, "URL::InitFromBuf(): scheme %s, buf[%ld]: %s.",
          scheme_.c_str(), (len - n), buf_ptr);
#endif

  // Second, grab the host (we *must* have a host!).
  char* ptr = &scratch_buffer[0];
  while (n > 0 && *buf_ptr != ':' && *buf_ptr != '/' && !isspace(*buf_ptr)) {
    *ptr++ = *buf_ptr++;
    n--;
  }
  *ptr = '\0';  // null terminate the host

  // TODO(aka) Hmm, it's conceivable that our buffer may have stopped
  // being populated at this point, i.e., we could have some data that
  // is part of a host and think we're all done.  Perhaps we should
  // attempt to *resolve* the host, to see if we got a fully-qualified
  // host name?

  set_host(scratch_buffer);

#if DEBUG_PARSE
  _LOGGER(LOG_NORMAL, "URL::InitFromBuf(): host %s, buf[%ld]: %s.",
          host_.c_str(), (len - n), buf_ptr);
#endif

  // Next, check to see if we had a port.
  if (n > 0 && *buf_ptr == ':') {
    // We have a port, so get it.
    buf_ptr++;	// skip over ":"
    if (--n == 0)
      return 0;  // not enough data yet (we continued process with ':')
    char* ptr = &scratch_buffer[0];
    while (n > 0 && isdigit(*buf_ptr)) {
      *ptr++ = *buf_ptr++;
      n--;
    }
    *ptr = '\0';  // null terminate the port
    set_port((in_port_t)strtoul(scratch_buffer, (char**)NULL, 10));	
  } else {
    set_port(default_port);
  }

#if DEBUG_PARSE
  _LOGGER(LOG_NORMAL, "URL::InitFromBuf(): port %hu, buf[%ld]: %s.",
          port_, (len - n), buf_ptr);
#endif

  // And if we have a path.
  if (n > 0 && *buf_ptr == '/') {
    // We have a path, so get it.
    buf_ptr++;	// skip over "/"
    if (--n == 0)
      return 0;  // not enough data yet (we continued process with '/')
    char* ptr = &scratch_buffer[0];
    while (n > 0 && *buf_ptr != kQueryStart &&
           *buf_ptr != kFragmentStart && !isspace(*buf_ptr)) {  // query & frags
      *ptr++ = *buf_ptr++;
      n--;
    }
    *ptr = '\0';  // null terminate the path
    set_path(scratch_buffer, strlen(scratch_buffer));
  }

#if DEBUG_PARSE
  _LOGGER(LOG_NORMAL, "URL::InitFromBuf(): path %s, buf[%ld]: %s.",
          path, (len - n), buf_ptr);
#endif

  // See if we have a query.
  if (n > 0 && *buf_ptr == kQueryStart) {
    // We have a query, so get it.
    buf_ptr++;	// skip over "?"
    if (--n == 0)
      return 0;  // not enough data yet (we continued process with '?')
    char* ptr = &scratch_buffer[0];
    while (n > 0 && *buf_ptr != kFragmentStart &&
           !isspace(*buf_ptr)) {  // until we reach a fragment
      *ptr++ = *buf_ptr++;
      n--;
    }
    *ptr = '\0';  // null terminate the query
    set_query(scratch_buffer, strlen(scratch_buffer));
  }

#if DEBUG_PARSE
  _LOGGER(LOG_NORMAL, "URL::InitFromBuf(): query %s = %s, buf[%ld]: %s.",
          query_.key.c_str(), query_.value_c_str(), (len - n), buf_ptr);
#endif

  // Finally, see if we have a fragment.
  if (n > 0 && *buf_ptr == kFragmentStart) {
    // We have a fragment, so get it.
    buf_ptr++;	// skip over "#"
    if (--n == 0)
      return 0;  // not enough data yet (we continued process with '#')
    char* ptr = &scratch_buffer[0];
    while (n > 0 && !isspace(*buf_ptr)) {
      *ptr++ = *buf_ptr++;
      n--;
    }
    *ptr = '\0';  // null terminate the fragment
    set_fragment(scratch_buffer);
  }

#if DEBUG_PARSE
  _LOGGER(LOG_NORMAL, "URL::InitFromBuf(): END buf[%ld]: %s.",
          (len - n), buf_ptr);
#endif

  return (len - n);
}

// URL manipulation functions.
	
// Routine to compare *just* the IP tuple.  We return '1' if the URLs match!
int URL::CompareTuples(const URL& other, const int exact_port) const {

  // Note, if "exact_port" is 0, we only compare the ports if *both*
  // are set.  If exact_port is non-zero then we compare the ports
  // ridgedly, i.e., if it's not set in one, then they don't match!

  string tmp_str;

  // If the host names don't match, game over man!
  if (host_.compare(other.host_)) {
    // Okay, they don't match, but let's try the in_addr_arpa ...
    string ip1 = get_reverse_dns(host_.c_str());
    string ip2 = get_reverse_dns(other.host_.c_str());

    // _LOGGER(LOG_DEBUGGING, "CompareTuples(): comparing dotted quads: %s, %s.", ip1.c_str(), ip2.c_str());

    if (! ip1.size() || ! ip2.size() || ip1.compare(ip2))
      return 0;
  }

  if (exact_port) {
    // If the ports don't match, game over!
    if (port_ != other.port_)	
      return 0;
  } else {
    // If *both* ports are set, *and* they don't match, game over!
    if (port_ != URL_PORT_NULL && other.port_ != URL_PORT_NULL &&
        port_ != other.port_)
      return 0;
  }

  return 1;	// we matched
}


// Boolean check functions.

// Obsolete code.
#if 0
// Assignment operator function.
URL& URL::operator =(const URL& src) 
{
  // Increment src's ref count and decrement ours (this).
  src.url->cnt++;	// doing this first protects against 'src = src'
  if (! --url->cnt)
    this->~URL();	// destruct object

  url = src.url;

  return *this;
}

// Overloaded operator: equality function.
int URL::operator ==(const URL& other) const 
{ 
  // This is an *exact* match function, if you are looking for a more
  // Internet friendly fuction, check URL::Compare_Tuple()!

  return (*host_ == *other.host_ && 
          *path_ == *other.path_ && 
          port_ == other.port_) ? 1 : 0; 
}
#endif
