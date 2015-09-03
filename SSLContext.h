// $Id: SSLContext.h,v 1.4 2005/05/31 16:56:39 akadams Exp $

// SSL Context Functions: wrapper for initializing an OpenSSL context.
// Copyright (c) 1998, see the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _SSLCONTEXT_H_
#define _SSLCONTEXT_H_

#include <openssl/ssl.h>

#include <string>
using namespace std;

#include "File.h"


// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.
#define SSLCONTEXT_DEFAULT_VERIFY_DEPTH 2	   // 0:peer + 1:CA + 2:CA
#define SSLCONTEXT_DEFAULT_RAND_MAX_BYTES -1

// Non-class specific utilities.
string ssl_err_str(void);
//const int ssl_check_version(void);
//int pem_passwd_cb(char* buf, int size, int rwflag, void* userdata);


/** Class for controlling SSL instantiation.
 *
 *  A more detailed decription of the template.
 *
 *  RCSID: $Id: SSLContext.h,v 1.2 2012/05/22 16:33:27 akadams Exp $.
 *
 *  @author Andrew K. Adams <akadams@psc.edu>
 */
class SSLContext {
 public:
  /** Constructor.
   *
   */
  SSLContext(void);

  /** Destructor.
   *
   */
  virtual ~SSLContext(void);

  // Copy constructor, assignment and equality operator needed for STL.

  // Accessors.
  // XXX const SSL_CTX* ctx(void) const { return ctx_; }
  // XXX const SSL_METHOD* method(void) const { return method_; }

  // Mutators.

  // SSLContext manipulation.

  /** Routine to initialize a SSLContext object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed.  This routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   */
  void Init(const SSL_METHOD* method, const char* session_id, 
            const char* keyfile_name,  const char* keyfile_dir, 
            const int keyfile_type, const char* password, 
            const char* certfile_name, const char* certfile_dir, 
            const int certfile_type,
            const int verify_mode, const int verify_depth,
            const long cache_mode, const long options);

#if 0  // TODO(aka)
  void Set_Cipher_List(const char* arg_cipherlist)
#endif

  // Certificate & key manipulation.
#if 0  // TODO(aka)
  void Load_Cert(const char* sandbox, const char* password,
                 int type = SSL_FILETYPE_PEM)
  throw (Exception_Handler);
  void Load_CA_List(const char* sandbox, 
                    const char* ca_subdir_name,
                    const char* ca_list_file = NULL)
  throw (Exception_Handler);
#endif

  // Crypto control functions.
#if 0  // TODO(aka)
  void Load_DH_Ephemeral_Keys(const char* sandbox, 
                              const char* dh_file_name
                              = "dh_param_1024.pem",
                              const char* dh_file_subdir = "")
  void Seed_PRNG(const char* egd_path, 
                 const char* sandbox, 
                 const char* rand_file_name, 
                 const char* rand_file_subdir = "",
                 const long max_bytes 
                 = SSL_CONTEXT_DEFAULT_RAND_MAX_BYTES)
#endif

  // Flags.

  friend class SSLConn;

 protected:
  // Data members.
  SSL_CTX* ctx_;	// OpenSSL context (link to SSL context(s))
#if 0
  File keyfile_;	// filename of private key
  File certfile_;	// filename of certificate
  SSL_METHOD* method_;	// e.g., SSLv3, TLSv1, SSLv23
  int verify_mode_;	// mode to do certificate verification
  int verify_depth_;	// number of chained certs to traverse
#endif

  // Unique session id, represented as *any* binary expression.
  char session_id_[SSL_MAX_SSL_SESSION_ID_LENGTH];  // TODO(aka) Should this be a string?

  // int (*passwd_cb)(char* buf, int size, int rwflags, 
  // 		void* userdata);	// ptr to call-back function

  unsigned int cnt_;    // for reference counting this object (used by parent)

 private:
  // Dummy declarations for copy constructor and assignment & equality operator.

  /** Copy constructor.
   *
   */
  SSLContext(const SSLContext& src);

  /** Assignment operator.
   *
   */
  SSLContext& operator =(const SSLContext& src);

  /** Equality operator.
   */
  int operator ==(const SSLContext& other) const;
};


#endif
