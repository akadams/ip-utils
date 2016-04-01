// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef _TCPSESSION_H_
#define _TCPSESSION_H_

#include <pthread.h>
#include <stdint.h>

#include <string>
#include <queue>
#include <vector>
using namespace std;

#include "File.h"
#include "SSLConn.h"
#include "MsgHdr.h"      // type of framing used
#include "MsgInfo.h"

// Forward declarations (used if only needed for member function parameters).

// Non-class specific defines & data structures.

// Non-class specific utilities.

/** Class to manage a SSL/TLS or unencrypted TCP/IP session.
 *
 *  The TCPSession class manages a SSL/TLS session or a straight
 *  TCP/IP session.  This includes; providing staging (either memory
 *  buffers or files) to hold data either to be read from or written
 *  to the socket (witin the SSLConn or TCPConn base-class).
 *  Moreover, It uses the MsgHdr class to determine when it has in its
 *  possession the *complete* header information (i.e., framing), such
 *  that it can commence processing of the the message.
 *
 *  The TCPSession class *should be* thread safe, i.e., any instance,
 *  e.g., a connection to a specific host, can process multiple
 *  incoming or outgoing messages in parallel.  This is accomplished
 *  by having all operations on the internal incoming and outgoing
 *  buffers (and meta-data) require a MUTEX lock within the TCPSession
 *  class.  There is a separate lock for incoming and outgoing
 *  operations.
 *
 *  Notes:
 *
 *  - This Class treats the connection the same, i.e., it's up to
 *    SSLConn to defer to TCPConn if using TCP.  This is usually
 *    accomplished by checking for a NULL SSL* object in the SSLConn
 *    routines.
 *
 *  RCSID: $Id: TCPSession.h,v 1.3 2014/04/11 17:42:15 akadams Exp $
 *
 *  @see SSLConn
 *  @see MsgHdr
 *
 *  @author Andrew K. Adams <akadams@psc.edu>
 */
class TCPSession : public SSLConn {
 public:
  /** Constructor.
   *
   */
  explicit TCPSession(const uint8_t framing_type);

  /** Destructor.
   *
   */
  virtual ~TCPSession(void);

  /** Copy constructor, needed for STL.
   *
   */
  TCPSession(const TCPSession& src);

#if 0  // XXX
  /** Equality operator, needed for STL.
   *
   */
  int operator ==(const TCPSession& other) const;
#endif

  // Accessors.
  uint8_t framing_type(void) const { return framing_type_; }
  uint16_t handle(void) const { return handle_; }
  uint8_t synchronize_status(void) const { return synchronize_status_; }
  char* rbuf(void) const { return rbuf_; }
  ssize_t rbuf_size(void) const { return rbuf_size_; }
  ssize_t rbuf_len(void) const { return rbuf_len_; }
  File rfile(void) const { return rfile_; }
  MsgHdr rhdr(void) const { return rhdr_; }
  const MsgInfo rpending(void) const { return rpending_; }
  pthread_t rtid(void) const { return rtid_; }

  char* wbuf(void) const { return wbuf_; }
  ssize_t wbuf_size(void) const { return wbuf_size_; }
  ssize_t wbuf_len(void) const { return wbuf_len_; }
  int wbuf_cnt(void) const { return wpending_.size(); }
  list<MsgHdr> whdrs(void) const { return whdrs_; }

  // Mutators.
  void set_handle(const uint16_t handle);
  void set_synchronize_status(const uint8_t synchronize_status);
  void set_rfile(const char* path, const ssize_t len);
  void set_rtid(const pthread_t rtid);
  void set_connected(const bool connected);

  /** Routine to set the storage type in rpending_.
   *
   *  Besides setting the storage value, we also (and more
   *  importantly) set the storage_initialzied flag, as well.
   *
   */
  void set_storage(const int storage);

  /** Routine to remove a MsgHdr from our write-headers (whdrs_).
   *
   */
  void delete_whdr(const uint16_t msg_id);

  /*
  // XXX Deprecated.
  void set_rhdr(const uint16_t msg_id, const HdrStorage& hdr);
  void set_rhdr(const MsgHdr& rhdr);
  void add_whdr(const MsgHdr msg_hdr);
  list<MsgHdr>::iterator delete_whdr(list<MsgHdr>::iterator msg_hdr);
  */

  // TCPSession manipulation.

  /** Routine to *pretty-print* an object (usually for debugging).
   *
   */ 
  string print(void) const;

  /** Routine to initialize the internal memory buffers within a TCPSession.
   *
   *  Work beyond what is suitable for the class constructor needs to
   *  be performed.  This routine will set an ErrorHandler event if it
   *  encounters an unrecoverable error.
   *
   *  @see ErrorHandler
   */
  void Init(void);

  /*
  // XXX Deprecated.
  void InitRpending(const uint16_t msg_id, 
                    const ssize_t hdr_len, const ssize_t body_len, 
                    const uint8_t storage, const char* path);
  */

  /** Routine to setup an incoming message's meta-data.
   *
   *  If we have enough data in our memory buffer (rbuf_) to build a
   *  complete message header, we copy the framing header to our
   *  internal holder (rhdr_), build our message's meta-data
   *  (rpending_), and remove the framing header from our incoming
   *  buffer (rbuf_).
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @see ErrorHandler
   *  @param base_name a char* representing the base-name of the streaming file
   *
   */
  bool InitIncomingMsg(void);

  /*
  // XXX Deprecated.
  ssize_t AddMsgHdr(const char* msg_hdr, const ssize_t hdr_len, 
                    const ssize_t body_len, const uint16_t msg_id,
                    const uint8_t storage);  // ErrorHandler
  void AppendMsgBody(const char* msg_body, const ssize_t body_len);
  void AppendMsgFile(const File& file);
  */

  /** Routine to add a message (in memory) to our outgoing queue.
   *
   *  We load the MsgHdr into our whdrs_ list, add the framing header
   *  (msg_hdr) & msg_body to our wbuf_ buffer, update wbuf_'s control
   *  varibles and build our outgoing message's meta-data (wpending_).
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @param whdr is a MsgHdr representing framing header of the message
   */
  bool AddMsgBuf(const char* framing_hdr, const ssize_t hdr_len, 
                 const char* msg_body, const ssize_t body_len,
                 const MsgHdr& whdr);

  /** Routine to add a message (in a file) to our outgoing queue.
   *
   *  We load the MsgHdr into our whdrs_ list, add the framing header
   *  (msg_hdr) to our wbuf_ buffer, update wbuf_'s control varibles,
   *  add the file (msg_body) to our wfiles_ list, and build our
   *  outgoing message's meta-data (wpending_).
   *
   *  Note, this routine can set an ErrorHandler event.
   *
   *  @param whdr is a MsgHdr representing framing header of the message
   */
  bool AddMsgFile(const char* framing_hdr, const ssize_t hdr_len, 
                  const File& msg_body, const ssize_t body_len, 
                  const MsgHdr& whdr);

  ssize_t Read(bool* eof);  // ErrorHandler
  ssize_t Write(void);  // ErrorHandler

  int StreamIncomingMsg(void);  // ErrorHandler
  // void ShiftRpending(void);
  void ClearIncomingMsg(void);
  // void ShiftWpending(void);
  void PopOutgoingMsgQueue(void);

  // Boolean checks.
  bool IsSynchroniationEnabled(void) const { return synchronize_connection_; }
  bool IsIncomingMsgInitialized(void) const {
    return (rpending_.initialized == 1) ? true : false; }
  bool IsIncomingStorageInitialized(void) const { 
    return rpending_.storage_initialized; }
  bool IsIncomingDataStreaming(void) const { 
    return (rpending_.storage == SESSION_USE_DISC) ? true : false; }
  bool IsIncomingMsgComplete(void) const {
    return ((rpending_.initialized == 1) && 
            ((rpending_.file_offset >= 
              rpending_.body_len) ||
             (rbuf_len_ >= rpending_.body_len))) ? true : false;
  }
  bool IsIncomingMsgBeingProcessed(void) const {
    return (rtid_ > 0) ? true : false; }
  bool IsOutgoingMsgSent(void) const {
    return (wpending_.size() && 
            ((wpending_.front().buf_offset >=
              (wpending_.front().hdr_len + wpending_.front().body_len)) ||
             ((wpending_.front().file_offset + 
               wpending_.front().buf_offset) >=
              (wpending_.front().hdr_len + wpending_.front().body_len)))) ?
        true : false;
  }
  bool IsOutgoingDataPending(void) const;

  // Flags.

 protected:
  // Data members.
  uint8_t framing_type_;        // framing type of TCP/IP session (@see MsgHdr)
  uint16_t handle_;             // session id (for easier STL access)

  bool synchronize_connection_; // flag to show we want to synchronize session
  uint8_t synchronize_status_;  // flag to show what stage the framing
                                // protocol is in

  char* rbuf_;                  // incoming read buffer
  ssize_t rbuf_size_;           // maximum size of rbuf
  ssize_t rbuf_len_;            // marks end of data in rbuf
  File rfile_;                  // File object to stream incoming data to
  MsgInfo rpending_;            // message meta-data for pending read data
  MsgHdr rhdr_;                 // the parsed message-header of current msg
  pthread_t rtid_;              // identifier of thread handeling the
                                // next available incoming message (or
                                // 0 if single-threaded)

  char* wbuf_;                  // outgoing write buffer
  ssize_t wbuf_size_;           // maximum size of wbuf
  ssize_t wbuf_len_;            // marks end of data in wbuf
  vector<File> wfiles_;         // list of files to be sent out
  vector<MsgInfo> wpending_;    // queue of struct MsgInfo for each write
                                // message pending

  list<MsgHdr> whdrs_;          // archived message-headers of sent
                                // REQUESTS (kept around to associate
                                // with incoming RESPONSES).

 private:
  // Private routines do *not* explicitly set MUTEX locks, hence, they
  // are only called internally by public TCPSession routines that
  // *have* already set the appropriate lock.

  //vector<MsgInfo> wpending(void) const { return wpending_; }
  //MsgInfo rpending(void) const { return rpending_; }
  void ShiftRbuf(const ssize_t len, const ssize_t offset);
  void ResetRbuf(void);
  void ResetWbuf(void);

  pthread_mutex_t incoming_mtx;  // lock for rbuf_ & friends
  pthread_mutex_t outgoing_mtx;  // lock for wbuf_ & friends

  /*
  // Deprecated locks, as we now lock all incoming or outgoing data
  // members when processing a message (incoming or outgoing,
  // respectivley).

  pthread_mutex_t rbuf_len_mtx;
  pthread_mutex_t rbuf_mtx;
  pthread_mutex_t rbuf_size_mtx;
  pthread_mutex_t rfile_mtx;
  pthread_mutex_t rpending_mtx;
  pthread_mutex_t rhdr_mtx;
  pthread_mutex_t wbuf_mtx;
  pthread_mutex_t wbuf_size_mtx;
  pthread_mutex_t wbuf_len_mtx;
  pthread_mutex_t wfiles_mtx;
  pthread_mutex_t wpending_mtx;
  pthread_mutex_t whdrs_mtx;
  */

  // Dummy declarations for copy constructor and assignment & equality operator.
  TCPSession& operator =(const TCPSession& src);

  /** Equality operator, needed for STL.
   *
   */
  int operator ==(const TCPSession& other) const;
};


#endif  /* #ifndef _TCPSESSION_H_ */
