/* $Id: SSLContext.cc,v 1.1 2012/02/03 13:17:10 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <openssl/err.h>

#include <string>
using namespace std;

#include "Logger.h"
#include "SSLContext.h"

// Non-class specific defines & data structures.

// Non-class specific utility functions.
#if 0
const int ssl_check_version(void)
{
#if OPENSSL_VERSION_NUMBER < 0x0090607fL
	return 0;
#else
	return 1;
#endif
}
#endif

// Routine to return a string of SSL error messages.
string ssl_err_str(void) {
  string tmp_str(1024, '\0');  // '\0' so strlen works

  unsigned long ssl_error = ERR_get_error();
  while (ssl_error) {
    ERR_error_string_n(ssl_error, 
                       (char*)tmp_str.c_str() + strlen(tmp_str.c_str()),
                       1024 - strlen(tmp_str.c_str()));
    ssl_error = ERR_get_error();

    // If there's another one, add a space first.
    if (ssl_error && (1024 - strlen(tmp_str.c_str()) > 0))
      strncat((char*)tmp_str.c_str(), " ", 1);
  }

  return tmp_str;
}

static int pem_passwd_cb(char* buf, int size, int rwflag, void* userdata) {
  // Default Routine called when loading/storing a PEM certificate 
  // with encryption.
  //
  // Currently, we only move the supplied password 'userdata' to
  // the buffer used by OpenSS 'buf'.

  memset(buf, 0, (size_t)size);
  strncpy(buf, (char*)userdata, size);
  buf[size - 1] = '\0';	// in-case userdata is > size

  _LOGGER(LOG_DEBUG, "pem_passwd_cb(): received password: %s, with flags: %d.",
          userdata, rwflag);

  return strlen(buf);
}


// SSLContext Class.

// Constructors and destructor.
SSLContext::SSLContext(void) {
  ctx_ = NULL;
  // XXX method_ = NULL;
  memset((void*) session_id_, 0, SSL_MAX_SSL_SESSION_ID_LENGTH);
  //  mode_ = 0;
  //  depth_ = 0;
  cnt_ = 1;
}

SSLContext::~SSLContext(void) {
  ERR_free_strings();	// SSL_load_error_strings(3)
  EVP_cleanup();		// SSL_add_all_ciphers|digests(3)

  if (ctx_ != NULL) {
    SSL_CTX_free(ctx_);
    ctx_ = NULL;
  }

 // XXX method_ = NULL;
}

// Copy constructor, assignment and equality operator, needed for STL.

// Accessors.

// Mutators.

// SSLContext manipulation.
void SSLContext::Init(const SSL_METHOD* method, const char* session_id, 
                      const char* keyfile_name,  const char* keyfile_dir, 
                      const int keyfile_type, const char* password, 
                      const char* certfile_name, const char* certfile_dir, 
                      const int certfile_type,
                      const char* CAfile, const char* CApath,
                      const int verify_mode, const int verify_depth, 
                      const long cache_mode, const long options) {
  if (method == NULL) {
    error.Init(EX_SOFTWARE, "SSLContext::Init(): SSL_METHOD* is NULL");
    return;
  }
  // XXX method_ = method;

  SSL_load_error_strings();  // give us readable errors
  SSL_library_init();
	
  // TODO(aka) Do we now need to handle SIGPIPE?

  // Setup a SSL context with the method we want.
  if ((ctx_ = SSL_CTX_new(method)) == NULL) {
    error.Init(EX_SOFTWARE, "SSLContext::Init(): SSL_CTX_new() failed: %s", 
               ssl_err_str().c_str());
    return;
  }

  // TODO(aka): Figure out how to get the ASCII version of a method
  // from a SSL_METHOD* ... might have to query Eric Rescorla on this
  // one.

  _LOGGER(LOG_INFO, "SSLContext::Init(): "
          "Started new SSL context with method: %#x.", method->version);

  // Set how this SSL context will behave.  Choices are:
  //
  // SSL_MODE_ENABLE_PARTIAL_WRITE
  // SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER
  // SSL_MODE_AUTO_RETRY

  SSL_CTX_set_mode(ctx_, SSL_MODE_ENABLE_PARTIAL_WRITE);

  // TOOD(aka) We may want to enable SSL_MODE_AUTO_RETRY, but as we
  // probably are not going to run in BLOCKING more, and I'm not sure
  // if a client and server method needs to be explicitly set, we'll
  // wait.

  // Set the unique session id and distinguish this session from other
  // applications.

  strncpy(session_id_, session_id, SSL_MAX_SSL_SESSION_ID_LENGTH - 1);
  session_id_[SSL_MAX_SSL_SESSION_ID_LENGTH - 1] = '\0';  // just in-case
  if (!SSL_CTX_set_session_id_context(ctx_, (unsigned char*)session_id_, 
                                      strlen(session_id_))) {
    error.Init(EX_SOFTWARE, "SSLContext::Init(): "
               "SSL_CTX_set_session_id_context() failed: %s",
               ssl_err_str().c_str());
    return;
  }

  if (certfile_name != NULL && strlen(certfile_name) > 0) {
    // Load in host certificate.  Valid certificate file types are:
    //
    // SSL_FILETYPE_PEM
    // SSL_FILETYPE_ASN1
 
    File certfile;
    certfile.Init(certfile_name, certfile_dir);
    if (!certfile.Exists(NULL)) {
      error.Init(EX_SOFTWARE, "SSLContext::Init(): "
                 "certfile: %s, does not exist!", certfile.path(NULL).c_str());
      return;
    }

    // TODO(aka) According to SSL_CTX_use_certificate(3), we should be
    // using SSL_CTX_use_certificate_chain_file() here!

    if (!SSL_CTX_use_certificate_file(ctx_, certfile.path(NULL).c_str(),
                                      certfile_type)) {
      error.Init(EX_SOFTWARE, "SSLContext::Init(): "
                 "SSL_CTX_use_certificate_file() failed: %s",
                 ssl_err_str().c_str());
      return;
    }

    // If we also specified a private key to use with our cert, load it.
    if (keyfile_name != NULL && strlen(keyfile_name) > 0) {
      File keyfile;
      keyfile.Init(keyfile_name, keyfile_dir);
      if (!keyfile.Exists(NULL)) {
        error.Init(EX_SOFTWARE, "SSLContext::Init(): "
                   "keyfile: %s, does not exist!", keyfile.path(NULL).c_str());
        return;
      }

      // If the user specified a password, pass it to our callback.
      if (password != NULL && strlen(password) > 0) {
        // TODO(aka) What we want is a fuction to reassign the default
        // callback 'pem_passwd_cb' to another user supplied function!

        _LOGGER(LOG_DEBUG, "SSLContext::Init(): TODO(aka) password callback not impleneted yet!");

        // Set callback function to prompt for password & load the one we have.
        SSL_CTX_set_default_passwd_cb(ctx_, pem_passwd_cb);
        SSL_CTX_set_default_passwd_cb_userdata(ctx_, (void*)password);
      }

      // Load key via a STDOUT command prompt.

      // TODO(aka) It would be nice if there was someway that I could tell
      // when OpenSSL *was* going to prompt for a password ...

      // fprintf(stdout, "%s X.509 passphrase: ", certfile.path(NULL).c_str());
      fflush(stdout);

      if (!SSL_CTX_use_PrivateKey_file(ctx_, keyfile.path(NULL).c_str(),
                                       keyfile_type)) {
        error.Init(EX_SOFTWARE, "SSLContext::Init(): "
                   "SSL_CTX_use_PrivateKey_file() failed: %s",
                   ssl_err_str().c_str());
        return;
      }
    
      // Check the key.
      if (!SSL_CTX_check_private_key(ctx_)) {
        error.Init(EX_SOFTWARE, "SSLContext::Init(): "
                   "SSL_CTX_check_private_key() failed: %s",
                   ssl_err_str().c_str());
        return;
      }
    }  // if (keyfile_name != NULL && strlen(keyfile_name) > 0) {

    // TODO(aka): Do we need to load a DSA key as well (and a dsa signed cert)?

    _LOGGER(LOG_NOTICE, "Loaded certfile: %s.", certfile.path(NULL).c_str());
  }  // if (certfile_name != NULL && strlen(certfile_name) > 0) {
  
  // If the user gave us a CAfile or CApath, load the CAs.
  if ((CAfile != NULL && strlen(CAfile)) ||
      (CApath != NULL && strlen(CApath))) {
    if (!SSL_CTX_load_verify_locations(ctx_, CAfile, CApath)) {
      error.Init(EX_SOFTWARE, "SSLContext::SSL_CTX_load_verify_locations(): "
                 "CAfile (%s), CApath(%s): %s", CAfile, CApath,
                 ssl_err_str().c_str());
      return;
    }
  }

  // Set the maximum number of chained certificates to be used 
  // during the verification process:
  // 
  // 	level 0: peer certificate
  //	level 1: CA certificate
  //	level 2: (higher) CA certificate
  //
  // As well as the 'verification' mode, including:
  //
  // SSL_VERIFY_NONE                    (server & client)
  // SSL_VERIFY_PEER                    (server & client)
  // SSL_VERIFY_FAIL_IF_NO_PEER_CERT    (server only)
  // SSL_VERIFY_CLIENT_ONCE             (server only)

  //  verify_mode_ = arg_mode;
  SSL_CTX_set_verify(ctx_, verify_mode, NULL);	// set the verification mode

  //  verify_depth_ = verify_depth;
  SSL_CTX_set_verify_depth(ctx_, verify_depth);

  // Set how we want to handle reusable sessions (see SSL_CTX_set_session_cache_mode(3)).
  SSL_CTX_set_session_cache_mode(ctx_, cache_mode);

  // Set any options (a bitmask, see SSL_CTX_set_options(3)).
  if (options > 0)
    SSL_CTX_set_options(ctx_, options);  // TODO(aka) if we care, new options are returned ...
}

// Boolean checks.

// Non-class specific utility functions.


#if 0
void SSL_Context::Set_Cipher_List(const char* arg_cipherlist)
	throw (Exception_Handler)
{
	// Sets the 'list' of available ciphers for this context.

	if (arg_cipherlist && strlen(arg_cipherlist))
		if (! SSL_CTX_set_cipher_list(ctx, arg_cipherlist))
			error.Init(EX_SOFTWARE, "SSLContext::SSL_CTX_set_cipher_list(): "
						"Unable to set list to: %s",
						arg_cipherlist);

	return;
}
#endif

// Certificate manipulation fuctions.
#if 0
void SSL_Context::Load_CA_List(const char* sandbox, 
			       const char* ca_dir_name, 
			       const char* ca_file_name)
	throw (Exception_Handler)
{
	// Routine to load CA certs.
	//
	// Although OpenSSL allows one to put all the CAs in one file, 
	// i.e., CAfile is non NULL, we've decided to go with the more 
	// canonical method of using one file per CA.

	if (! ca_dir_name && ! strlen(ca_dir_name))
		error.Init(EX_SOFTWARE, "SSLContext::SSL_Context::Load_CA_List(): "
					"CA dir name is empty or NULL!");

	Directory ca_dir(ca_dir_name);

	if (! ca_dir.Exists(sandbox))
		error.Init(EX_SOFTWARE, "SSLContext::SSL_Context::Load_CA_List(): "
					"CA directory: %s, doesn't exist!", 
					ca_dir.Path());

	// Load CA we trust.
	if (! SSL_CTX_load_verify_locations(ctx, NULL, ca_dir.path(NULL).c_str()))
		error.Init(EX_SOFTWARE, "SSLContext::SSL_CTX_load_verify_locations(): %s",
					ssl_err_str().c_str());

	// Hmm, according to SSL_CTX_load_verify_locations(3), the above 
	// routine does *not* influence the list of CA's sent to the client,
	// when requesting a client certificate.  Hence, we must 
	// *explicitly* set those now.

	// So, we need to loop through all files in CApath and get the certs!
	
	// Note, most of these non SSL_Context calls can throw an exception.
	int cnt = 0;
	ca_dir.Open(sandbox);
	ca_dir.Rewind();
	String filename = 
		ca_dir.Get_Next_Entry(sandbox, Directory::FILES_ONLY);
	while (filename.Str_Len()) {
		// Note, if we have CRLs in the same directory, then we will
		// have to check the extension of the file, as well, (perhaps
		// loading them at this time makes sense also!)

		// Get the cert from this file.
		File ca_file(filename, ca_dir.Path());
		ca_file.Fopen(sandbox, "r");
		X509* cert = NULL;
		if (PEM_read_X509(ca_file.FP(), &cert, NULL, NULL)) {
			// Add it to our acceptable CA list.
			SSL_CTX_add_client_CA(ctx, cert);

			_LOGGER(LOG_DEBUG, 
				   "Adding CA cert %s to our client list.", 
				   ca_file.Path());

			cnt++;
		}
		
		ca_file.Fclose();

		// Get next file.
		filename = 
			ca_dir.Get_Next_Entry(sandbox, Directory::FILES_ONLY);
	}
	
	ca_dir.Close();

	_LOGGER(LOG_INFO, "Loaded %d CA(s) from directory: %s.", 
		   cnt, ca_dir.Path());

	return;
}

// Crypto control functions.
void SSL_Context::Load_DH_Ephemeral_Keys(const char* sandbox,
					 const char* dh_file_name,
					 const char* dh_file_dir)
	throw (Exception_Handler)
{
	// Read in DH parameters generated via:
	//
	// 	openssl dhparam -out dh_param_1024.pem -2 1024

	File dh_file(dh_file_name, dh_file_dir); 
	if (! dh_file.Exists(sandbox))
		error.Init(EX_SOFTWARE, "SSLContext::SSL_Context::Load_Ephemeral_Keys(): "
					"DH group params file: %s, "
					"doesn't exist!", dh_file.Print());

	dh_file.Fopen(sandbox);
	DH* dh = PEM_read_DHparams(dh_file.FP(), NULL, NULL, NULL);
	dh_file.Fclose();

	// Set SSL_OP_SINGLE_DH_USE, to ensure a new key with each negotiation.
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);

	// Set the DH group parameters
	if (! SSL_CTX_set_tmp_dh(ctx, dh))
		error.Init(EX_SOFTWARE, "SSLContext::SSL_CTX_set_tmp_dh(): "
					"DH group params install failed: %s",
					ssl_err_str().c_str());

	// If we cared, we probably should load 512 byte DH params as well ...

	_LOGGER(LOG_INFO, "Loaded DH group parameters from file: %s.",
		   dh_file.Print());

	return;
}

void SSL_Context::Seed_PRNG(const char* egd_path,
			    const char* sandbox, 
			    const char* rand_file_name, 
			    const char* rand_file_dir,
			    const long max_bytes)
	throw (Exception_Handler)
{
	// Routine to seed the PRNG.
	//
	// OpenSSL uses /dev/urandom by default, so if it exists, we 
	// shouldn't need to do anything in here.

	if (RAND_status() == 1)
		return;	// life is good, /dev/urandom exists

	// See if we have a .rnd file ...
	File rand_file(rand_file_name, rand_file_dir);
	if (! rand_file.Exists(sandbox))
		_LOGGER(LOG_WARN, "PRNG seed file: %s, doesn't exist.", 
			   rand_file.path(NULL).c_str());
	else {
		if (! RAND_load_file(rand_file.path(NULL).c_str(), max_bytes))
			error.Init(EX_SOFTWARE, "SSLContext::RAND_load_file(): "
						"Rand file read failed: %s",
						ssl_err_str().c_str());

		_LOGGER(LOG_INFO, "Loaded rand file: %s.", 
			   rand_file.path(NULL).c_str());

		if (! RAND_status())
			_LOGGER(LOG_WARN, 
				   "%s seed for PRNG is not good enough!",
				   rand_file.Name()->Print());
		else
			return;
	}

	// See if we have an Entropy Gathering Daemon ...
	// 
	// Note, this is one place where we don't use sandbox, as the socket 
	// could be a *system* file descriptor, i.e., outside the NIMI realm.

	if (egd_path) {
		if (RAND_egd(egd_path) == -1)
			_LOGGER(LOG_WARN, 
				   "EGD path: %s, doesn't exist, or it does "
				   "not contain enough entropy for PRNG "
				   "seed!", egd_path);
		else 
			_LOGGER(LOG_INFO, "Opened EGD socket: %s.", 
				   egd_path);

		if (! RAND_status())
			_LOGGER(LOG_WARN, 
				   "EGD seed for PRNG is not good enough!");
		else
			return;
	}

	// FUTURE: We probably should just (gracefully) die here!
	if (! RAND_status())
		_LOGGER(LOG_WARN, 
			   "Seed for PRNG *still* not good enough!");
		
	return;
}

#endif
