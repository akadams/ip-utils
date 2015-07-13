/* $Id: Logger.cc,v 1.8 2013/09/13 14:56:38 akadams Exp $ */

// Copyright (c) 2008, see the file 'COPYRIGHT.txt' for any restrictions.

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define SYSLOG_NAMES     // so we get SYSLOG priority levels from syslog.h
#include "Logger.h"


#undef DEBUG_SCRIPT_CHILD	// for reporting 'exec()' in our child process

#define INITIAL_BUF_SIZE 1024
#define MAX_BUF_SIZE (1024 * 100)
#define MAX_PATH_SIZE PATH_MAX	     // defined in limits.h
#define MAX_FILE_SIZE ((~(size_t)0) / 2)
#define MAX_ROLL_FILES 5

#define LOG_CONFIG_DIR "etc/"
#define LOG_FILE_EXT ".log"

static const char* mechanism_names[LOGGER_NUM_MECHANISMS] = {
	"null",
	"stderr",
	"stdout",
	"file",
	"console",
	"syslog",
	"script"
};

static const char* kLogPriorityNone = "none";

Logger logger;	// global definition, for all who use the logger

// Non-member local functions.
static void log_to_stderr(const char* msg);
static void log_to_stdout(const char* msg);
static void log_to_file(const char* msg, const char* path);
static void log_to_console(const char* msg);
static void log_to_syslog(const char* msg);
static pid_t log_to_script(const char* msg, const char* path);


// Non-member functions.

// Routine to return the 'int' value of an ASCII log priority
int get_log_priority(const char* priority_name) {
  if (priority_name == NULL || *priority_name == '\0')
    return LOG_INFO;  // something is broken, return something non-broken

  /*
  // Convert *priority_name* to lowercase, and truncate it if it's too long.
  int i;
  char tmp_buf[MAX_BUF_SIZE];
  char* buf_ptr = priority_name;
  for (i = 0; *buf_ptr != '\0' && i < MAX_BUF_SIZE - 1; i++) {
  tmp_buf[i] = tolower(*buf_ptr++);
  }
  tmp_buf[i] = '\0';
  */

  // Get the priority from CODE prioritynames[] in sys/syslog.h.
  int i = 0;
  while (prioritynames[i].c_name != NULL) {
    if (strlen(prioritynames[i].c_name) == strlen(priority_name) &&
        !strncasecmp(prioritynames[i].c_name, priority_name,
                    strlen(prioritynames[i].c_name)))
      return prioritynames[i].c_val;

    i++;
  }

  warnx("Logger::logger_get_log_priority(): "
        "No \'priority\' value found for priority %s", priority_name);

  return LOG_INFO;  // something is broken, return something non-broken
}

// Routine to return the a pointer to the priority name.
const char* get_log_priority_name(const int priority) {
  int i = 0;
  while (prioritynames[i].c_name != NULL) {
    if (prioritynames[i].c_val == priority)
      return prioritynames[i].c_name;

    i++;
  }

  warnx("Logger::logger_get_log_priority_name(): "
        "No \'priority name\' found for priority %d", priority);

  return kLogPriorityNone;
}

// Routine to return the 'int' value of an ASCII mechanism type.
log_mechanism_type get_log_mechanism_type(const char* mechanism_name) {
  if (mechanism_name == NULL || *mechanism_name == '\0')
    return LOG_TO_STDERR;  // something is broken, return something non-broken

  /*
  // Convert *mechanism* to lowercase, and truncate it if it's too long.
  int i = 0;
  char tmp_buf[MAX_BUF_SIZE];
  char* buf_ptr = mechanism;
  for (i = 0; *buf_ptr != '\0' && i < MAX_BUF_SIZE - 1; i++) {
  tmp_buf[i] = tolower(*buf_ptr++);
  }
  tmp_buf[i] = '\0';
  */

  // Find the mechanism in our data member.
  int i = 0;
  for (i = 0; i < LOGGER_NUM_MECHANISMS; i++) {
    if (strlen(mechanism_names[i]) == strlen(mechanism_name) &&
        !strncasecmp(mechanism_names[i], mechanism_name, 
                     strlen(mechanism_names[i])))
      return (log_mechanism_type)i;
  }

  warnx("Logger::logger_get_log_mechanism_type(): "
        "No \'type\' value found for mechanism %s!", mechanism_name);

  return LOG_TO_STDERR;  // something is broken, return something non-broken
}

// Routinen to return a pointer to the mechanism name.
const char* get_log_mechanism_name(const log_mechanism_type mech_id) {
  return mechanism_names[mech_id];
}

// Routine to lower-case a string.
string lc(const char* p) {
  size_t len = strlen(p) + 1;
  string lowercase_str(len, '\0');
  size_t cnt = 0;
  while (*p) {
    int c = tolower(*p++);
    snprintf((char*)lowercase_str.c_str() + cnt, len - cnt, "%c", c);
    cnt++;
  }

  return lowercase_str;
}


// Accessors.

// Routine to *pretty* print the object.
const string Logger::print(void) const {
  string tmp_buf(MAX_BUF_SIZE, '\0');
  snprintf((char*)tmp_buf.c_str(), MAX_BUF_SIZE, "%s", proc_name_.c_str());

  for (int i = 0; i < LOGGER_NUM_MECHANISMS; i++)
    snprintf((char*)tmp_buf.c_str() + tmp_buf.size(), 
             MAX_BUF_SIZE - tmp_buf.size(), 
             ":%d:%d", i, mechanisms_[i]);

  return tmp_buf;
}

// Mutators.
void Logger::set_proc_name(const char* proc_name) {
  proc_name_ = proc_name;
}

// TODO(aka) Not sure if we need this ...
/*
void Logger::set_log_filename(const char* arg_name, const char* arg_subdir) {
  log_file.setPath(arg_name, arg_subdir);
}
*/

// Routine to set the mechanism priority for each mechanism specified
// via the command line.  The parameter optarg *should* look like,
// e.g.,:
//
//	 stderr:normal,file=/etc/messages:quiet
//
void Logger::set_mechanism_priority(char* optarg) {
  // Note, the ',' is not supported yet, so you'll have to enter
  // multiple type:priority on the command line.

  // First, see if we have a ','.
  char* comma_ptr;
  if ((comma_ptr = strchr(optarg, ','))) {
    // TODO(aka) We need to loop on the number of ','s within optarg.
    errx(EXIT_FAILURE, "Logger::set_mechanism_priority(): "
         "',' not supported in command line processing yet: %s", optarg);
  }

  // See if we have a ':' separator.
  char* colon_ptr = NULL;
  char* priority_name_ptr = NULL;
  if ((colon_ptr = strchr(optarg, ':'))) {
    *colon_ptr = '\0';	// split optarg
    priority_name_ptr = colon_ptr + 1;  // we're guarenteed to get '\0' here
  }
	
  // And see if we have an '=' within our mechanism type.
  char* equal_sign_ptr = NULL;
  if ((equal_sign_ptr = strchr(optarg, '='))) {
    *equal_sign_ptr = '\0';	// split optarg (possibly again)
    char* arg_ptr = equal_sign_ptr + 1;
			
    // Depending on the mechanism type, handle the '=' arg.
    if (get_log_mechanism_type(optarg) == LOG_TO_SCRIPT) {
      // TODO(aka) Script processing not done yet.

      // It's a script, so set our script file.
      script_ = arg_ptr;
    } else if (get_log_mechanism_type(optarg) == LOG_TO_FILE) {
      log_file_path_ = arg_ptr;  // it's for file logging, set our path
    } else {
      // Else, we ignore what's after the '=' sign ...
      warnx("Logger::set_mechanism_priority(): %s \'=\' %s was set"
            ", but not supported in command-line processing, yet, ignoring ...",
            optarg, arg_ptr);
    }
  }

  // See if we specified a priority, i.e., was ':' set.
  if (priority_name_ptr) {
    // Set the mechanism to the priority specified.
    set_mechanism_priority(get_log_mechanism_type(optarg),
                           get_log_priority(priority_name_ptr));
  } else {
    // If we haven't already previously set the priority, use our
    // default priority. 

    // TODO(aka) Hmm, now that I think about it, it would be silly
    // (and arguably operator error) if the user specified a log
    // mechanism on the command line *and* didn't set the mechanism
    // priority *and* not expect this routine to overwrite the priority,
    // right?

    if (mechanism_priority(get_log_mechanism_type(optarg)) < 0)
      set_mechanism_priority(get_log_mechanism_type(optarg), LOG_INFO);
    else {
      // Well, technically, there's nothing to do, as the
      // mechanism has already been recorded *with* a priority!

      warnx("Logger::set_mechanism_priority(): "
            "Mechanism: %s, already has priority: %d set",
            optarg, mechanism_priority(get_log_mechanism_type(optarg)));
    }
  } 

  return;
}

// Routine to set the mechanism priority for each mechanism.
void Logger::set_mechanism_priority(const log_mechanism_type mechanism, 
                                    const int priority) {
  if (mechanism < LOGGER_NUM_MECHANISMS && mechanism >= 0) {
    if (priority >= 0 && priority <= LOG_DEBUG)
      mechanisms_[mechanism] = priority;
    else {
      warnx("Logger::set_mechanism_priority(%d, %d): "
            "priority out of bounds, setting to LOG_NONE.", 
            mechanism, priority);
      mechanisms_[mechanism] = LOG_NONE;
    }
  } else {
    warnx("Logger::set_mechanism_priority(%d, %d): "
          "mechanism out of bounds, ignoring.", 
          mechanism, priority);
  }
}

// Routine to decrement a mechanism's priority (probably due to
// processing a '-q' on the command line).
void Logger::DecrementMechanismPriority(void) {
  for (int i = 1; i < LOGGER_NUM_MECHANISMS; i++) {
    if (mechanisms_[i] >= 0)
      mechanisms_[i] = mechanisms_[i] - 1;
  }
}

// Routine to increment a mechanism's priority (probably due to
// processing a '-v' on the command line).
void Logger::IncrementMechanismPriority(void) {
  for (int i = 1; i < LOGGER_NUM_MECHANISMS; i++) {
    if (mechanisms_[i] >= 0)
      mechanisms_[i] = mechanisms_[i] + 1;
  }
}


// Logger manipulation functions.

// Initilization routine for *file* logging.
void Logger::InitLogFile(const char* sandbox, const char* subdir, 
                         const char* name, const char* ext) {
  // There may be utility in keeping the path segmented, but we need
  // to assemble the pieces here in-order to generate our file
  // pointer.

  string filename;
  if (ext && strlen(ext) > 0) {
    if (ext[0] == '.') {
      filename = name;
      filename += ext;
    } else {
      filename = name;
      filename += '.';
      filename += ext;
    }
  } else {
    filename = name;
  }

  string path = sandbox;

  // Make sure we've got a '/' between sandbox and subdir.
  if ((sandbox[strlen(sandbox) - 1] != '/') && (subdir[0] != '/')) {
    warnx("Logger::InitLogFile(): "
          "sandbox (%s) and subdir (%s) lacked necessary \'/\'.", 
          sandbox, subdir);
    path += '/';
  }
  path += subdir;

  // Make sure we've got a '/' between sandbox and subdir.
  if (subdir[strlen(subdir) - 1] != '/' && name[0] != '/') {
    warnx("Logger::InitLogFile(): "
          "subdir (%s) and name (%s) lacked necessary \'/\'.",
          subdir, name);
    path += '/';
  }
  path += filename;

  /*		
  // Permanently save the sandbox just so that we don't need
  // to keep passing it along with every call to Log().

  set_sandbox(sandbox);
  */

  // TODO(aka) I'd like to create a File Class that can *roll* log
  // files, and keep the sandbox, subdir and filename all
  // compartmentalized.

  /*
  // See if it exists, if so roll it, else "touch" it.
  if (file.Exists(sandbox)) {
    file.Roll(sandbox, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, MAX_ROLL_FILES);
    }
  }
  */

  log_file_path_ = path;  // set our data member
}

// TODO(aka) I'm not ready to debug an script init() routine, yet.
#if 0
// Routine to initialize the 'logging script' for the execv() command.
void Logger::initScriptFile(const char* sandbox) {
  // XXX Why don't we simply keep the sandbox in the Logger?
  // Build the 'logging script' path, and store it in our 'local' var.
  snprintf(script_command, MAX_PATH_SIZE, "%s%s", sandbox, script);

  // Make sure it exists.
  File script_file(script);
  if (! script_file.Exists(sandbox))
    errx(EXIT_FAILURE, "Logger::Script_Init(): %s does not exist!", script_command);

  // And make sure it's executable.
  if (! script_file.Executable(sandbox))
    errx(EXIT_FAILURE, "Logger::Script_Init(): %s is not executable!", script_command);

  return;
}
#endif

// Routine to log a message.  The message's priority is checked
// against *all* currently set mechanism in our data member array.  If
// a mechanism within our array has a set positive priority that is >=
// the message's priority, then we build the message.  After the
// message is built, we call log_output() for each mechanism that had
// a priority >= the message priority.
void Logger::Log(const int priority, const char* format, ...) {
  int i;
  for (i = 1; i < LOGGER_NUM_MECHANISMS; i++) {
    if (mechanism_priority((log_mechanism_type)i) >= priority)
      break;
  }
  if (i == LOGGER_NUM_MECHANISMS)
    return;  // no mechanisms for this priority level
			
  // get the current UTC time, convert it to an ASCII localtime.
  time_t now;
  if ((now = time(NULL)) == -1)
    now = 0;	// oh well, we get 1970 as our date
  struct tm* tm_ptr = localtime(&now);

  // Because asctime(3) adds a goddamn '\n', which we need to remove,
  // we'll use the _r version (so we can operate on *our* buffer).

  static char asctime_buf[64];	// must hold at least 26 bytes
  char* asctime_ptr = asctime_r(tm_ptr, asctime_buf);

  // Remove the goddamn '\n' from the ASCII time string.
  char* line_feed = index(asctime_ptr, '\n');
  *line_feed = '\0';

  // Add the message prefix based on the log priority.
  char msg_buf[MAX_BUF_SIZE + 1];
  memset(msg_buf, 0, MAX_BUF_SIZE);
  // TODO(aka) uppercase get_log_priority_name().
  snprintf(msg_buf, MAX_BUF_SIZE, "%s: %s: %s: ", 
           get_log_priority_name(priority), proc_name_.c_str(), asctime_ptr);

  // Add the variable length stuff if we have any.
  size_t len = strlen(msg_buf);
  if (format && strlen(format)) {
    va_list ap;
    va_start(ap, format);
    vsnprintf(msg_buf + len, MAX_BUF_SIZE - len, format, ap);
    va_end(ap);
  }

  len = strlen(msg_buf);
  snprintf(msg_buf + len, MAX_BUF_SIZE - len, "\n");  // add a line feed

  // For each mechanism >= "our log message priority", call log_MECHANISM().
  for (i = 1; i < LOGGER_NUM_MECHANISMS; i++) {
    if (mechanism_priority((log_mechanism_type)i) >= priority) {
      if (i == LOG_TO_STDERR) {
        log_to_stderr(msg_buf);
      } else if (i == LOG_TO_STDOUT) {
        log_to_stdout(msg_buf);
      } else if (i == LOG_TO_FILE) {
        log_to_file(msg_buf, log_file_path_.c_str());
      } else if (i == LOG_TO_CONSOLE) {
        log_to_console(msg_buf);
      } else if (i == LOG_TO_SYSLOG) {
        log_to_syslog(msg_buf);
      } else if (i == LOG_TO_SCRIPT) {
        log_to_script(msg_buf, "XXX/foo");  // TODO(aka) finish this!
      }
    }  
  }

  return;
}

// Main *logging* non-member functions.

// Routine to log a message to STDERR.
void log_to_stderr(const char* msg) {
  fprintf(stderr, "%s", msg);
}

// Routine to log a message to STDOUT, god knows why.
void log_to_stdout(const char* msg) {
  fprintf(stdout, "%s", msg);
}

// Routine to log a message to a file.
void log_to_file(const char* msg, const char* path) {
  // Get the file pointer to our log file.
  FILE* fp = fopen(path, "a");
  if (fp == NULL) {
    // TODO(aka) Die horribly here for now ...
    err(EXIT_FAILURE, "Logger::InitLogFile(): fopen(%s) failed", path);
  }

  // See if file needs rolled.
  struct stat info;
  int fd = fileno(fp);
  fstat(fd, &info);
  if (info.st_size >= (off_t)MAX_FILE_SIZE) {
    // TODO(aka) Uh, I don't have time to finish this now ...
    warnx("Logger::log_to_file(): "
          "log file %luB > %luB, but file rolling is not implemented, yet.", 
          (unsigned long)info.st_size, (unsigned long)MAX_FILE_SIZE);
  }

#if HAVE_FLOCK
#if LOG_FILE_LOCKING
  // Lock the file.
#endif
#endif
  fputs(msg, fp);
#if HAVE_FLOCK
#if LOG_FILE_LOCKING
  // Unlock the file.
#endif
#endif

  // Close the file.
  fclose(fp);
}

// Routine to log a message to CONSOLE.
void log_to_console(const char* msg) {
  // TODO(aka) What we really need here is a way to force syslog() to
  // write to /dev/console ...

  // Open /dev/console for writing, and spit out the message.
  FILE* fp;
  if ((fp = fopen("/dev/console", "r+")) == NULL) {
    // TODO(aka) Argh, need to die horribly here, as well.
    err(EXIT_FAILURE, "fopen(/dev/console) failed");
  } else {
    fprintf(fp, "%s", msg);
    fclose(fp);
  }
}

// Routine to log a message to syslog(3).
void log_to_syslog(const char* msg) {

  // TOOD(aka) Figure out how to pass facility into this routine.
  // *priority* should be our log priority.

  int priority = LOG_INFO | LOG_DAEMON;
  syslog(priority, msg, NULL);
}

// Routine to fork & exec a script.
pid_t log_to_script(const char* msg, const char* path) {

  // If we encounter an error anywhere in the parent, then we remove
  // LOG_TO_SCRIPT from our mechanism array, and re-call Log() to
  // *report* the error we encountered in here.  Note, that A) we will
  // be recursively calling Log(), and B) any mechanism types of a
  // 'higher' value than LOG_TO_SCRIPT *will* have any data that was
  // in the static buffers within Log() 'hosed' when eventually called
  // for the original message that was to be logged in here!

  // TODO(aka) Script logging has not been debugged, yet.
#if 0
  char* msg_buf = NULL;
  size_t msg_buf_size = INITIAL_BUF_SIZE;

  char* args[] = {
    (char*)script_command,
    NULL,
    NULL
  };

  // Add the 'logging script' command to our args array.
  if (! args[0]) {
    args[0] = (char*) malloc(strlen(script_command) + 1);
    if (! args[0]) {
      logger.clearMechanism(LOG_TO_SCRIPT);
      logger.Log(LOG_ERROR, 
                 "Logger::log_to_script(): malloc() failed!");

      return 0;
    }
  } else if (strlen(args[0]) < strlen(script_command)) {
    char* p = (char*) realloc(args[0], strlen(script_command) + 1);
    if (! p) {
      logger.clearMechanism(LOG_TO_SCRIPT);
      logger.Log(LOG_ERROR, 
                 "Logger::log_to_script(): "
                 "realloc() failed: %s", strerror(errno));

      return 0;
    } else
      args[0] = p;
  }

  strncpy(args[0], script_command, strlen(script_command) + 1);

  if (! msg_buf) {
    // Only preformed the first time in.
    if (! (msg_buf = (char*) malloc(msg_buf_size))) {
      logger.ClearMechanism(LOG_TO_SCRIPT);
      logger.Log(LOG_ERROR, "Logger::log_to_script(): "
                 "malloc() msg_buf failed!");

      return 0;
    }
  } 

  // Add the 'msg' to our args array.
  if (strlen(msg) > msg_buf_size - 1) {
    char* p = (char*) realloc(msg_buf, strlen(msg) + 1);
    if (p) {
      msg_buf = p;
      msg_buf_size = strlen(msg) + 1;
    } else {
      logger.ClearMechanism(LOG_TO_SCRIPT);
      logger.Log(LOG_WARN, "Logger::log_to_script(): "
                 "realloc(args[1]) failed, size: %d!",
                 strlen(msg));

      return 0;
    }
  }
		
  snprintf(msg_buf, msg_buf_size, "%s", msg);

  // Add the 'msg' to our args array.
  args[1] = &msg_buf[0];

  // Fork off child process.
  pid_t fork_pid;
  if ((fork_pid = fork()) < 0) {
    logger.clearMechanism(LOG_TO_SCRIPT);
    logger.Log(LOG_ERROR, 
               "Logger::log_to_script(): fork() failed: %s",
               strerror(errno));

    return 0;
  } else if (fork_pid == 0) {
    // We are the child!

    // FUTURE: If the following fails due to PATH, then we will
    // FUTURE: need to set it in the parent prior to the fork()!

#ifdef DEBUG_SCRIPT_CHILD
    // For debugging:
    char args_buf[MAX_BUF_SIZE];
    args_buf[0] = '\0';
    for (int i = 0; args[i]; i++) {
      strncat(args_buf, args[i], 
              MAX_BUF_SIZE - strlen(args_buf));
      strncat(args_buf, " ", MAX_BUF_SIZE - strlen(args_buf));
    }
    fprintf(stderr, "DEBUG: Executing: %s %s\n", args[0], args_buf);
#endif

    // Run the command.
    if ((execv(args[0], args)) == -1)
      err(EXIT_FAILURE, "Logger::log_to_script(): "
          "execv() failed: ");
  }

  // We are the parent!
  return fork_pid;
#endif

  return 0;
}

// TODO(aka) I'm not sure if we want the ability to *restart* ourselves, yet ...
#if 0
// Routine to output a string of args suitable for *re-initializing* a
// program using Logger.
const char* Logger::getArgsList(void) const {

  static char tmp_buf[MAX_BUF_SIZE];

  memset(tmp_buf, 0, MAX_BUF_SIZE);

  int need_space = 0;
  for (int i = 0; i < LOGGER_NUM_MECHANISMS; i++) {
    if (i == LOG_TO_SCRIPT)
      continue;	// for now, skip it

    if (mechanisms[i]) {
      if (need_space)
        strncat(tmp_buf, " ", MAX_BUF_SIZE - 1);

      snprintf(tmp_buf + strlen(tmp_buf), 
               MAX_BUF_SIZE - strlen(tmp_buf),
               "-L%s:%s", 
               get_log_mechanism_name((log_mechanism_type) 
                                      i), 
               get_log_priority_name(mechanisms[i]));

      need_space = 1;
    }
  }

  return &tmp_buf[0];	// caution: static buffer
}
#endif


