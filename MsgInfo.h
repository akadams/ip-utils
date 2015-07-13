/* $Id: MsgInfo.h,v 1.0 2015/02/03 13:17:10 akadams Exp $ */

// MsgInfo Class: structures for storing incoming/outgoing message
// meta-data in [TCP|TLS] Sessions.

// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef MSGINFO_H_
#define MSGINFO_H_


// Structure to hold meta-data for a pending incoming or outgoing
// message.  Similar to the MsgHdr Class (and works in conjunction
// with MsgHdr), this structure allows TLSSession or TLSSession to be
// framing-agnostic.  That is, we keep the necessary meta-data during
// a session in here for easier processing (e.g., so we know when we
// have the entire message for processing!).
//
// Note, MsgInfo != MsgHdr, MsgHdr describes/controls the framing,
// while MsgInfo describes the complete message (regardless of framing
// type)!
struct MsgInfo {
  uint8_t initialized;       // 1 if the meta-data has been initialized, 0 otherwise
  uint8_t storage;           // file or internal memory buffer
  bool storage_initialized;  // storage type was explicitly set
  uint16_t msg_id;           // unique id of message (used to associate a
                             // msg to its header)
  ssize_t hdr_len;           // size of msg header in memory buffer
                             // XXX TODO(aka) Actually, this is now simply a
                             // flag to show that the message-header has
                             // been parsed (into rhdr_) Uh, not so fast;
                             // hdr_len is indeed used *as* the header
                             // length in check_outgoing_msgs(), i.e.,
                             // wpending_!
  ssize_t body_len;          // size of file or length of msg body in
                             // internal memory buffer
  ssize_t buf_offset;        // offset in memory buffer marking what was
                             // sent/received so far
  ssize_t file_offset;       // offset in file to what has been sent or
                             // received so far
};

// Session (TCP, TLS) defines.
#define SESSION_DEFAULT_BUFSIZE 4096
#define SESSION_THREAD_NULL 0

enum { SESSION_USE_MEM, SESSION_USE_DISC };  // where we're storing messages

// TODO(aka) For compatability.
#define TCPSESSION_DEFAULT_BUFSIZE SESSION_DEFAULT_BUFSIZE
#define TCPSESSION_THREAD_NULL SESSION_THREAD_NULL
#define TCPSESSION_STORAGE_WBUF SESSION_USE_MEM
#define TCPSESSION_STORAGE_FILE SESSION_USE_DISC

// Non-class specific utilities.


#endif  /* #ifndef MSGINFO_H_ */
