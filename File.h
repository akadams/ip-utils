// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _FILE_H_
#define _FILE_H_

#if defined(__linux__)
#include <linux/limits.h>	// for PATH_MAX
#else
#include <sys/syslimits.h>	// for PATH_MAX
#endif

#include <fcntl.h>
#include <stdio.h>      	// for fileno, FILE* and FILENAME_MAX

#include <string>
using namespace std;

#include "ErrorHandler.h"
#include "Descriptor.h"

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.
#define FILE_MAX_PATH PATH_MAX
#define FILE_MAX_FILENAME FILENAME_MAX
#define FILE_TMP_DIR "tmp/"		// used in Roll() with MOVE_FILE flag

#define FILE_CHUNK_SIZE 1024 * 4

const int kFileChunkSize = FILE_CHUNK_SIZE;

// Non-class specific utility functions.
bool is_path_tainted(const char* path);
bool is_path_slash_terminated(const char* path);

/** Class for streaming & low-level file I/O.
 *
 *  The File class uses the Descriptor class to *safely* copy and
 *  assign File objects (by using Descriptor's file descriptor
 *  reference counting).
 *
 *  Note, this class uses an *explicit* sandbox, which may prove
 *  useful for secure file management.  If you don't need the utility
 *  of a sandbox, simply replace that parameter with NULL in any
 *  member function that requires it.  The class will then use either
 *  the absolute path specified within the class object, or the
 *  current working directory.
 *
 *  RCSID: $Id: File.h,v 1.2 2012/05/10 17:50:46 akadams Exp $
 *
 *  @see Descriptor
 *  @author Andrew K. Adams (akadams@psc.edu)
 */
class File {
 public:
  /** Constructor.
   *
   */
  File(void);

  /** Destructor.
   *
   */
  virtual ~File(void);

  /** Copy constructor.
   *
   */
  File(const File& src);

  /** Assignment operator (needed for use with the STL).
   *
   */
  File& operator =(const File& src);

  /** Equality operator (needed for use with the STL).
   *
   */
  int operator ==(const File& other) const;

  // Accessors.
  string name(void) const { return name_; }
  string dir(void) const { return dir_; }

  /** Routine to return the low-level file descriptor in the File object.
   *
   *  @return int specifying the file descriptor within the Descriptor class
   */
  const int fd(void) const { return descriptor_->fd_; }

  /** Routine to return the streaming file descriptor in the File object.
   *
   *  TOOD(aka) This *should* return a const FILE*, but that would
   *  require us to write the methods, e.g., fgets(), to process the
   *  FILE*!
   *
   *  @return FILE* specifying the file descriptor within the Descriptor class
   */
  FILE* fp(void) const { return descriptor_->fp_; }

  /** Routine to return the full path to the File object.
   *
   *  If a sandbox is given, it is used as the root of the File
   *  object's path, i.e., it is appended to the returned path.
   *
   *  @param sandbox char* specifying the root path to the File object (or NULL)
   *  @return string specifying the full-path of the File object
   */
  string path(const char* sandbox) const;

  // XXX int Ffd(void) const { return fileno(fp); }

  /** Routine to return the size of the file pointed to by the File object.
   *
   *  @return off_t specifying the size of the file
   */
  off_t size(const char* sandbox) const;

  // Mutators.

  /** Routine to set filename.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   */
  void set_name(const char* name);

  /** Routine to set the directory path.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   */
  void set_dir(const char* dir);

  /** Routine to set the low-level file descriptor data member.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param fd an int specifying the file descriptor of the file
   */
  void set_fd(const int fd);

  /** Routine to set the streaming FILE pointer data member.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param fp a FILE* specifying the descriptor of the file
   */
  void set_fp(FILE* fp);

  void clear(void);

  // File manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** Initialize a File object.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed.  This routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param name a char pointer specifing the name (or path) of the file
   *  @param dir a char pointer specifying the path of the file (or NULL)
   *  @see ErrorHandler
   */
  void Init(const char* name, const char* dir);

  /** Routine to parse (or initialize) a File object from a char stream.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param buf a char* of data used to create File object
   *  @param len a ssize_t specifying the amount of data in buf
   *  @return a ssize_t specifying either the amount of data used in buf or 0
   */
  ssize_t InitFromBuf(const char* buf, const ssize_t len);

  /** Routine to open(2) a low-level File object.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param sandbox char* specifying the root path to the File object (or NULL)
   *  @param flags int specifying flags controling open operation
   *  @param mode mode_t specifying permissions if flag O_CREAT is used
   */
  void Open(const char* sandbox, const int flags, mode_t mode);

  /** Routine to close(2) a low-level File object.
   *
   */
  void Close(void);


  /** Routine to fopen(3) a streaming File object.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param sandbox char* specifying the root path to the File object (or NULL)
   *  @param mode char* specifying flags controlling fopen operation
   */
  void Fopen(const char* sandbox, const char* mode);

  /** Routine to fclose(3) a streaming File object.
   *
   */
  void Fclose(void); 

  /** Routine to unlink(2) the file within File object.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param sandbox char* specifying the root path to the File object (or NULL)
   */
  void Unlink(const char* sandbox);

  /** Routine to rename(2) the file within the File object.
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param sandbox char* specifying the root path to the File object (or NULL)
   *  @param newname char* specifying the new name (or path)
   *  @param newdir char* specifying the new dir (or NULL)
   */
  void Rename(const char* sandbox, const char* newname, const char* newdir);

  /** Routine to make an on-disk copy of a file.
   *
   *  TOOD(aka) It's arguable that this routine should *open* the
   *  original, as we probably don't want to allow differnt
   *  sandboxesfor the files ...
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param sandbox char* specifying the root path to the File object (or NULL)
   *  @param newname char* specifying the new name (or path or NULL) 
   *  @param newdir char* specifying the new dir (or NULL)
   */
  void Copy(const char* sandbox, const char* newname, const char* newdir);

  // XXX void Flock(int operation);  // apply lock to open file

  // Boolean checks.
  bool IsOpen(void) const;                       // true if file is open
  bool IsStdin(void) const;                      // true if fd is 0
  bool IsExecutable(const char* sandbox) const;  // true if it's executable
  bool Exists(const char* sandbox) const;        // true 1 if file exists

  // Flags.
  // XXX enum { NO_BACKUP, ROLL_FILE, MOVE_FILE, };

  friend class TCPSession;  // so TCPSession can stream to/from files

 protected:
  // Data members.
  Descriptor* descriptor_;  // file descriptor (or FILE*) object
  string name_;	            // name of file
  string dir_;              // directory of file; if sandbox != NULL, then subdirectory
  // XXX int backup_mode_;         // okay to backup, or move (to $TMP), or not!

 private:
  // Dummy declarations.
};


#endif  /* #ifndef _FILE_H_ */
