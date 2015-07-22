/* $Id: File.cc,v 1.4 2012/05/10 17:50:46 akadams Exp $ */

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>        // for random(3)

#include "ErrorHandler.h"
#include "File.h"

#define DEBUG_CLASS 0

#define SCRATCH_BUF_SIZE (FILE_CHUNK_SIZE * 2)

static int max_fd = 0;
//static int close_error = 0;


// Non-class specific utility functions.
bool is_path_tainted(const char* path) {
  if (path == NULL)
    return false;

  for (int i = 0; path[i] != '\0'; i++) {
    // Check to see if we have any unpure characters.
    if (path[i] == '`' || 
        path[i] == '|' || 
        path[i] == '\\')
      return true;
  }

  return false;  // we made it here, so the string's probably not tainted
}

bool is_path_slash_terminated(const char* path) {
  // If we have a '/' as our last character, return 1.
  if (path == NULL || (path[0] == '\0') || (path[strlen(path) - 1] != '/'))
    return false;
  else
    return true;
}

string gen_random_string(size_t len) {
  static char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  char* tmp_buf = NULL;

  if ((tmp_buf = (char*)malloc(len + 1)) == NULL)
    return "";

  for (int i = 0; i < (int)len; i++) {            
    int index = random() % (int)(sizeof(charset) -1);
    tmp_buf[i] = charset[index];
  }
  tmp_buf[len] = '\0';

  string tmp_str = tmp_buf;
  return tmp_str;
}


// File Class.

// Constructor.
File::File(void) 
    : name_(), dir_() {
#if DEBUG_CLASS
  warnx("File::File(void) called.");
#endif

  descriptor_ = new Descriptor();
  //backup_mode_ = ROLL_FILE;
}

// Destructor.
File::~File(void) {
#if DEBUG_CLASS
  warnx("File::~File(void) called, cnt: %d, fd: %d.", 
        descriptor_->cnt_, descriptor_->fd_);
#endif

  if (!--descriptor_->cnt_) {
    if (descriptor_->fd_ != DESCRIPTOR_NULL) {
      Close();
    } else if (descriptor_->fp_ != NULL) {
      Fclose();
    }
    
    delete descriptor_;
  }
}

// Copy constructor, assignment and equality operator, needed for STL.
File::File(const File& src) 
    : name_(src.name_), dir_(src.dir_) {
#if DEBUG_CLASS
  warnx("File::File(const File&) called, src cnt: %d, fd: %d.", 
        src.descriptor_->cnt_, src.descriptor_->fd_);
#endif

  // Since our Descriptor data member is just a pointer, we simply set
  // it to point to the source, then bump its reference count.

  descriptor_ = src.descriptor_;
  descriptor_->cnt_++;

  //backup_mode_ = src.backup_mode_;
}

// Assignment operator, needed for STL.
File& File::operator =(const File& src) {
#if DEBUG_CLASS
  warnx("File::opertator =(const File&) called, cnt: %d, fd: %d, src cnt: %d, fd: %d.", 
        descriptor_->cnt_, descriptor_->fd_, 
        src.descriptor_->cnt_, src.descriptor_->fd_);
#endif 

  name_ = src.name_;
  dir_ = src.dir_;
  //backup_mode_ = src.backup_mode_;

  // If we're about to remove our last instance of the Descriptor we
  // currently point to, clean it up first!

  if (!--descriptor_->cnt_) {
    if (descriptor_->fd_ != DESCRIPTOR_NULL) {
      Close();
    } else if (descriptor_->fp_ != NULL) {
      Fclose();
    }
    delete descriptor_;
  }

  // Now, since our Descriptor data member is just a pointer, we can
  // (re-)associate it to source's, then bump its reference count.

  descriptor_ = src.descriptor_;
  descriptor_->cnt_++;

  return *this;
}

// Equality operator, needed for STL.
int File::operator ==(const File& other) const { 
#if DEBUG_CLASS
  warnx("File::operator ==(const File&) called.");
#endif

  // A File is the same if it has the same path, regardless whether
  // or not the descriptor is open or not.

  return (name_ == other.name_ && dir_ == other.dir_) ? 1 : 0; 
}

// Accessors.

// Routine to build the path (if sandbox is non-null, it is prepended
// to the path).
string File::path(const char* sandbox) const {
  string tmp_str(FILE_MAX_PATH, '\0');  // so strlen() works

  if (sandbox && strlen(sandbox))
    snprintf((char*)tmp_str.c_str(), FILE_MAX_PATH, "%s%s%s",
             sandbox, dir_.c_str(), name_.c_str());
  else 
    snprintf((char*)tmp_str.c_str(), FILE_MAX_PATH, "%s%s",
             dir_.c_str(), name_.c_str());

  return tmp_str;
}

// Routine to return the size of the file, but handles all exceptions
// locally.
off_t File::size(const char* sandbox) const {
  if (name_.size() == 0)
    return 0;

  struct stat info;

  // See if the file is open.
  if (descriptor_->fp_ || descriptor_->fd_ != DESCRIPTOR_NULL) {
    // fstat(2) the file.
    int tmp_fd = (descriptor_->fp_ != NULL) 
        ? fileno(descriptor_->fp_) : descriptor_->fd_;
    if (fstat(tmp_fd, &info))
      return 0;  // fstat(2) failed
  } else {
    // stat() the file using File::path() to create the path.
    if (stat(path(sandbox).c_str(), &info))
      return 0;  // stat(2) failed
  }

  return info.st_size;	// return the st_size element
}

// Mutators.
void File::set_name(const char* name) {
  if (name == NULL) {
    error.Init(EX_SOFTWARE, "File::set_name(): name is NULL");
    return;
  }

  if (is_path_tainted(name)) {
    error.Init(EX_DATAERR, "File::set_name(): path is tainted");
    return;
  }

  if (!strncmp("-", name, 1))
    name_ = "stdin";  // HACK: to handle alias for stdin
  else 
    name_ = name;

  // Next, check if we have a trailing '/', remove it.  It's a
  // *file*, whether or not it's a directory makes no difference
  // in the File Class!

  if (name_[name_.size() - 1] == '/')
    name_.erase(name_.size() - 1, 1);
	
  // Now, see if we have a path as opposed to just a filename.
  // size_t slash_pos = strcspn(name_.c_str(), "/");
  size_t slash_pos = name_.find_last_of('/');
  if (slash_pos < name_.size()) {

    // Copy the name_ element to the dir element, blow away
    // the last filename (name_) from the dir, re-terminate
    // the dir, blow away the prefix in name_, re-terminate
    // the name.

    dir_ = name_;
    dir_.erase(slash_pos + 1);
    name_.erase(0, slash_pos + 1);
  }
}

void File::set_dir(const char* dir) {
  if (dir == NULL) {
    error.Init(EX_SOFTWARE, "File::set_dir(): dir is NULL");
    return;
  }

  dir_ = dir;
  if (!is_path_slash_terminated(dir_.c_str()))
    dir_ += "/";
}

void File::set_fd(const int fd) {
  if (descriptor_->fp_ != NULL) {
    error.Init(EX_SOFTWARE, "File::set_fd(): fp is not NULL");
    return;
  }

  if (descriptor_->fd_ != DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "File::set_fd(): fd is not NULL");
    return;
  }

  descriptor_->fd_ = fd; 
}

void File::set_fp(FILE* fp) {
  if (descriptor_->fd_ != DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "File::set_fp(): fd is not NULL");
    return;
  }

  if (descriptor_->fp_ != NULL) {
    error.Init(EX_SOFTWARE, "File::set_fp(): fp is not NULL");
    return;
  }

  descriptor_->fp_ = fp; 
}

void File::clear(void) {
#if DEBUG_CLASS
  warnx("File::clear(void) called, cnt: %d, fd: %d.", 
        descriptor_->cnt_, descriptor_->fd_);
#endif 

  name_.clear();
  dir_.clear();
  //backup_mode_ = ROLL_FILE;

  // If we're about to remove our last instance of the current
  // Descriptor, clean up the Descriptor object.

  if (!--descriptor_->cnt_) {
    if (descriptor_->fd_ != DESCRIPTOR_NULL) {
      Close();
    } else if (descriptor_->fp_ != NULL) {
      Fclose();
    }
    delete descriptor_;
  }

  descriptor_ = new Descriptor();
}

// File manipulation functions.

// Routine to *pretty-print* a File object.
string File::print(void) const {
  string tmp_str(FILE_MAX_PATH, '\0');  // so strlen() works

  if (descriptor_->fp_ != NULL)
    snprintf((char*)tmp_str.c_str(), FILE_MAX_PATH, "%s%s:%p", 
             dir_.c_str(), name_.c_str(), (void*)descriptor_->fp_);
  else if (descriptor_->fd_ != DESCRIPTOR_NULL)
    snprintf((char*)tmp_str.c_str(), FILE_MAX_PATH, "%s%s:%d", 
             dir_.c_str(), name_.c_str(), descriptor_->fd_);
  else
    snprintf((char*)tmp_str.c_str(), FILE_MAX_PATH, "%s%s", 
             dir_.c_str(), name_.c_str());

  return tmp_str;
}

// Routine to initialize a File objecct.
void File::Init(const char* name, const char* dir) {
  set_name(name);

  if (dir != NULL)
    set_dir(dir);
  else
    dir_.clear();
}

// Routine to build a File class object from an ASCII character
// stream, returning any unneeded data.
ssize_t File::InitFromBuf(const char* buf, const ssize_t len) {
  if (buf == NULL) {
    error.Init(EX_SOFTWARE, "File::InitFromBuf(): buf is NULL");
    return 0;
  }

  // First, right off the bat check for *stdin* before setting anything.
  if (!strncmp("-", buf, 1) || 
      !strncasecmp("stdin", buf, strlen("stdin"))) {
    name_ = "stdin";
    return 1;  // we have nothing else to do
  } 

  // Hmm, now we have 2 possibilities, either we have a simple
  // filename in buf, or we have path (e.g., foo/bar) in buf.

  // TOOD(aka) HACK, let's assume the former and shove our buf into name_.
  name_ = buf;

  // In any event, if we have a trailing '/', remove it.  It's a
  // *file*, whether or not it's a directory makes no difference
  // in the File Class!

  if (name_[name_.size() - 1] == '/')
    name_.erase(name_.size() - 1, 1);
	
  // Next, see if we have a path as opposed to just a filename.
  // size_t slash_pos = strcspn(name_.c_str(), "/");
  size_t slash_pos = name_.find_last_of('/');
  if (slash_pos < name_.size()) {

    // Copy the name_ element to the dir element, blow away
    // the last filename (name_) from the dir, re-terminate
    // the dir, blow away the prefix in name_, re-terminate
    // the name.

    dir_ = name_;
    dir_.erase(slash_pos + 1);
    name_.erase(0, slash_pos + 1);
  } else {	
    // We just have a *name* and due to the above HACK, it's already set.
    dir_.clear(); 
  }

  return strlen(buf);
}

// Routine to perform an low-level open(2).
void File::Open(const char* sandbox, const int flags, const mode_t mode) {
  if (descriptor_->fp_ != NULL) {
    error.Init(EX_SOFTWARE, "File::Open(): fp is not NULL");
    return;
  }

  if (descriptor_->fd_ != DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "File::Open(): fd is not NULL");
    return;
  }

  if (name_.size() == 0) {
    error.Init(EX_SOFTWARE, "File::Open(): name is empty");
    return;
  }

  if (name_ == "stdin") {
    descriptor_->fd_ = 0;  // set our Descriptor to stdin
    return;
  }

  // Use File::path() to create the path, and then open the file.
  if ((descriptor_->fd_ = open(path(sandbox).c_str(), flags, mode)) < 0) {
    error.Init(EX_IOERR, "File::Open(%s, %d, %d) failed: %s", 
                   path(sandbox).c_str(), flags, mode, strerror(errno));
    return;
  }

  // For Debugging:
  if (descriptor_->fd_ > max_fd)
    max_fd = descriptor_->fd_;
}

// Routine to close(2) a low-level file descriptor.
void File::Close(void) {
#if DEBUG_CLASS
      warnx("File::Close() called.");
#endif
  if (name_.size() == 0) {
    error.Init(EX_SOFTWARE, "File::Close(): name is empty");
    return;
  }

  if (descriptor_->fp_ != NULL) {
    error.Init(EX_SOFTWARE, "File::Close(): fp is not NULL");
    return;
  }

  if (descriptor_->fd_ == DESCRIPTOR_NULL)
    return;  // simply return if the file is already closed

  if (name_ == "stdin")
    return;  // stdin, so return

  // Close the file.
  if (close(descriptor_->fd_))
    warnx("File::Close(%d) filed: %s.", descriptor_->fd_, strerror(errno));

  descriptor_->fd_ = DESCRIPTOR_NULL;  // marks the file as closed
}

// Routine to open a file for streaming I/O with fopen(3).
void File::Fopen(const char* sandbox, const char* mode) {
  if (name_.size() == 0) {
    error.Init(EX_SOFTWARE, "File::Fopen(): name is empty");
    return;
  }

  if (descriptor_->fd_ != DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "File::Fopen(): fd is not NULL");
    return;
  }

  if (descriptor_->fp_ != NULL)
    return;  // simply return if the file is already open

  if (name_ == "stdin") {
    descriptor_->fp_ = stdin;  // set our Descriptor to stdin
    return;
  }

  // Use File::path() to create the path, and then open the file.
  if ((descriptor_->fp_ = fopen(path(sandbox).c_str(), mode)) == 0) {
    error.Init(EX_IOERR, "File::Fopen(%s, %s) failed: %s", 
                   path(sandbox).c_str(), mode, strerror(errno));
    return;
  }

  // Debugging stuff.
  int tmp_fd = fileno(descriptor_->fp_);  // get file discriptor
  if (tmp_fd > max_fd)
    max_fd = tmp_fd;
}

// Routine to close a file opened for streaming I/O.
void File::Fclose(void) {
#if DEBUG_CLASS
      warnx("File::Fclose() called.");
#endif

  if (name_.size() == 0) {
    error.Init(EX_SOFTWARE, "File::Fclose(): name is empty");
    return;
  }

  if (descriptor_->fd_ != DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "File::Fclose(): fd is not NULL");
    return;
  }

  if (descriptor_->fp_ == NULL)
    return;  // simply return if the file is already closed

  if (name_ == "stdin")
    return;  // stdin, so return

  // Close the file.
  if (fclose(descriptor_->fp_))
    warnx("File::Fclose(%p) failed: %s.", (void*)descriptor_->fp_, strerror(errno));

  descriptor_->fp_ = NULL;  // marks the file as closed
}

// Routine to call unlink(2).
void File::Unlink(const char* sandbox) {
  if (name_.size() == 0) {
    error.Init(EX_SOFTWARE, "File::Unlink(): name is empty");
    return;
  }

  // First, check to see if the file is open.
  if (descriptor_->fp_ != NULL)
    Fclose();
  else if (descriptor_->fd_ != DESCRIPTOR_NULL)
    Close();

  // TOOD(aka) Hmm, will the above work correctly even with the
  // reference counting in Descriptor?
	
  // Use File::getPath() to create the path, then "unlink" it.
  if (unlink(path(sandbox).c_str()))
    error.Init(EX_IOERR, "File::Unlink(): unlink(%s) failed: %s", 
               path(sandbox).c_str(), strerror(errno));
}

// Routine to rename a file.
void File::Rename(const char* sandbox, const char* newname, 
                  const char* newdir) {
  if (name_.size() == 0) {
    error.Init(EX_SOFTWARE, "File::Rename(): name is empty");
    return;
  }

  // See if the file is open first.
  if (descriptor_->fp_ != NULL)
    Fclose();
  else if (descriptor_->fd_ != DESCRIPTOR_NULL)
    Close();
	
  // TOOD(aka) Again, will the above be a problem with the reference
  // counting in Descriptor?

  // Generate the path to the "old" or existing file.
  string old_file_path = path(sandbox);

  // Now, determine if we are changing the file, dir or both.
  if (newname && strlen(newname))
    name_ = newname;  // set the new name_

  if (newdir && strlen(newdir)) {
    dir_ = newdir;  // set the new dir
    if (!is_path_slash_terminated(dir_.c_str()))
      dir_ += "/";
  }

  // Generate the path to the "new" file & rename it.
  string new_file_path = path(sandbox);
  if (rename(old_file_path.c_str(), new_file_path.c_str()))
    error.Init(EX_IOERR, "File::Rename(): rename(%s,%s) failed: %s", 
               old_file_path.c_str(), new_file_path.c_str(), 
               strerror(errno));
}

// Routine to make a low-level copy of the contents of a File object.
void File::Copy(const char* sandbox, const char* newname, const char* newdir) {
  if (descriptor_->fd_ == DESCRIPTOR_NULL) {
    error.Init(EX_SOFTWARE, "File::Copy(): fd is NULL");
    return;
  }

  // Now, determine if we are changing the file or the dir; as this
  // is a copy, we can't change both (as we could with Rename()).

  File copy;
  if (newname && strlen(newname)) {
    // We're going to make a new object in the same directory.

    // XXX TOOD(aka) We need to test for a path here ...
    copy.Init(newname, dir_.c_str());
  } else if (newdir && strlen(newdir)) {
    // We're keeping the name, just moving directories.
    copy.Init(name_.c_str(), newdir);
  } else {
    error.Init(EX_SOFTWARE, "File::Copy(): newname and newdir were NULL.");
    return;
  }

  // Open our new copy of the file.
  copy.Open(sandbox, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);

  // Loop over original file reading chunks and writing them to our
  // new file ...

  unsigned char tmp_buf[FILE_CHUNK_SIZE];
  ssize_t n;
  while ((n = read(descriptor_->fd_, tmp_buf, FILE_CHUNK_SIZE)) > 0) {
    ssize_t m = 0;
    while ((m = write(copy.descriptor_->fd_, tmp_buf + m, n - m)) > 0)
      ;
    if (m < 0) {
      error.Init(EX_IOERR, "File::Copy(): write(%s,%ld) failed: %s", 
                 copy.path(sandbox).c_str(), n, strerror(errno));
      return;
    }
  }
  if (n < 0) {
    error.Init(EX_IOERR, "File::Copy(): read(%s) failed: %s", 
               path(sandbox).c_str(), strerror(errno));
    return;
  }

  copy.Close();
}


// Boolean checks.
bool File::IsOpen(void) const {
  if (descriptor_->fp_ || descriptor_->fd_ != DESCRIPTOR_NULL)
    return true;
	
  return false;
}

bool File::IsStdin(void) const {
  if ((descriptor_->fp_ && descriptor_->fp_ == stdin) || descriptor_->fd_ == 0)
    return true;
	
  if (name_ == "-" || name_ == "stdin")
    return true;

  return false;
}

bool File::IsExecutable(const char* sandbox) const {
  if (name_.size() == 0)
    return false;

  // Use File::path() to create the path, then stat it.
  struct stat tmp_info;
  int return_value = stat(path(sandbox).c_str(), &tmp_info);
  if (return_value)
    return false;  // ERROR: don't bother with any checks
  else {
    if (tmp_info.st_mode & S_IXUSR)
      return true;
  }

  return false;
}

bool File::Exists(const char* sandbox) const {
  if (name_.size() == 0)
    return false;

  // Use File::path() to create the path, then stat it.
  struct stat tmp_info;
  int return_value = stat(path(sandbox).c_str(), &tmp_info);
  if (return_value)
    return false;  // ERROR: don't bother with any checks
  else
    return true;
}

