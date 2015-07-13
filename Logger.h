/* $Id: Logger.h,v 1.4 2013/05/07 13:45:29 akadams Exp $ */

// Logger Class: library for logging messages.

// Copyright © 2009, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <sys/syslog.h>     // for SYSLOG priority levels

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

#include <string>
using namespace std;

#ifdef USE_LOGGER
#define _LOGGER(priority, ...) logger.Log(priority, __VA_ARGS__)
#else
#pragma GCC diagnostic push  // TODO(aka) Alas, looks like we can't diag ignored a single #define!
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#define _LOGGER(priority, ...)
#pragma GCC diagnostic pop
#endif

// TODO(aka) Temporary support for deprecated Logger levels.
#define LOG_FATAL     LOG_EMERG
#define LOG_ERROR     LOG_ERR
#define LOG_NETWORK   LOG_ERR
#define LOG_REMOTE    LOG_ERR
#define LOG_WARN      LOG_WARNING
#define LOG_QUIET     LOG_WARNING
#define LOG_NORMAL    LOG_NOTICE
#define LOG_VERBOSE   LOG_INFO
#define LOG_DEBUGGING LOG_DEBUG

/* TODO(aka) Deprecated.
typedef enum {
	LOG_NONE,	// just a place holder for the array mechanisms[]
	LOG_FATAL,	// code fails, can't continue (usually in an init())
	LOG_ERROR,	// code fails & returns from current processing
	LOG_NETWORK,	// error *with* connection, can *not* report to REMOTE!
	LOG_REMOTE,	// LOG_ERROR that occured on a remote machine
	LOG_WARN,	// code is able to compensate and chug-along
	LOG_QUIET,	// minimal amount of "state" logging
	LOG_NORMAL,	// normal amount "state" logging
	LOG_VERBOSE,	// maximum amount of "state" logging
	LOG_DEBUGGING,	// okay, I lied, *this* is the maximum state logging
} log_level_type;	// possible log levels, default is LOG_NORMAL
*/

// Definition to show no priority set.
#define LOG_NONE -1

// Definitions of logger mechanisms.
typedef enum {
	LOG_TO_NULL,	// disables logging
	LOG_TO_STDERR,	// log to stderr
	LOG_TO_STDOUT,	// log to stdout ... hey, whatever
	LOG_TO_FILE,	// log to base_path/CONF_DIR/LOG_FILE (by default)
	LOG_TO_CONSOLE,	// log to /dev/console
	LOG_TO_SYSLOG,	// log to syslog()
	LOG_TO_SCRIPT	// pass message to script for processing
} log_mechanism_type;	// how to process a "particular" type of log message

#define LOGGER_NUM_MECHANISMS (LOG_TO_SCRIPT + 1)
#define LOGGER_PROC_NAME_MAX_SIZE 64

// Non-class specific utilities.
int get_log_priority(const char* priority_name);
const char* get_log_priority_name(const int priority);
log_mechanism_type get_log_mechanism_type(const char* mechanism);
const char* get_log_mechanism_name(const log_mechanism_type mech_id);
string lc(const char* p);


class Logger {
 public:
  // Constructor.
  Logger(void) {
    for (int i = 0; i < LOGGER_NUM_MECHANISMS; ++i)
      mechanisms_[i] = -1;
    mechanisms_[LOG_TO_STDERR] = LOG_NOTICE;  // default (until initialized)
    initialized_ = 0;
    debugging_ = 0;
    debug_mechanism_ = LOG_TO_STDERR;
    errors_fatal_ = 0;
  }

  // Using implicit destructor, copy constructor & assignment operator.

  // Accessors & mutators.
  string proc_name(void) const { return proc_name_; }
  //const char* getSandbox(void) const { return sandbox; }
  int mechanism_priority(const log_mechanism_type mechanism) const { 
    if (mechanism < LOGGER_NUM_MECHANISMS && mechanism >= 0)
      return mechanisms_[mechanism]; 
    else
      return mechanisms_[LOG_TO_NULL]; 
  }

  //const char* GetArgsList(void) const;
  const string print(void) const;

  void set_proc_name(const char* proc_name);
  //void set_log_filename(const char* arg_name, const char* arg_subdir);
  //void set_sandbox(const char* arg_sandbox);
  void set_errors_fatal(void) { errors_fatal_ = 1; }
  void set_initialized(void) { initialized_ = 1; }
  void set_mechanism_priority(char* optarg);
  void set_mechanism_priority(const log_mechanism_type mechanism, 
                              const int priority);
  void clear_mechanism(const log_mechanism_type mechanism) {
    if (mechanism < LOGGER_NUM_MECHANISMS && mechanism >= 0)
      mechanisms_[mechanism] = LOG_NONE;
  }

  void DecrementMechanismPriority(void);
  void IncrementMechanismPriority(void);

  // Logger manipulation.
  void InitLogFile(const char* sandbox, const char* subdir, 
                   const char* name, const char* ext);
  // void InitLogScript(const char* sandbox);

  void Log(const int priority, const char* format, ...); 
	
  // Boolean checks.
  const int AreErrorsFatal(void) const { return errors_fatal_; }  // TODO(aka) Deprecated

 protected:
  // Data members.
  int initialized_;             // flag to mark that configurations are done

  string proc_name_;    	// name of process 

  string log_file_path_;	// path to log file
  // string sandbox_;	        // cache for sandbox (for File class)
  FILE* fp_;                    // cache for file pointer of above log file

  // TODO(aka) I should probably make this script_file_path to match the log file.
  string script_;		// name of 'logging script'
  string script_command_;	// full path of file to execv()

  // Okay, we have an array of logging *mechanisms*, such that, we can
  // set the log level *per* mechanism!

  int mechanisms_[LOGGER_NUM_MECHANISMS];

  int errors_fatal_;	// flag to mark that *any* ERROR is fatal --
                        // TODO(aka) for debugging, now deprecated

  int debugging_;	// flag to mark a debugging run (1 = enabled)
  log_mechanism_type debug_mechanism_;	// where to log debug messages

 private:
};

extern ::Logger logger;  // declaration of global logger object


#endif  /* #ifndef _LOGGER_H_ */
