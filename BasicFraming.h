// Copyright © 2010, Pittsburgh Supercomputing Center (PSC).  
// See the file 'COPYRIGHT.txt' for any restrictions.

#ifndef BASICFRAMING_H_
#define BASICFRAMING_H_

#include <sys/types.h>

// Message passing framing.
struct BasicFramingHdr {
  uint8_t id;        // sender's id
  uint8_t type;      // type of message
  uint8_t type_id;   // some types of messages have an event id
                     // associated with it
  uint8_t opt;       // for now, this is the server's queue size
  uint16_t msg_id;   // unique sequence number of this message
  uint16_t lamport;  // lamport time on sending process
  uint16_t len;      // length of msg body; this needs to be increased
                     // at some point
};
const int kBasicHdrSize = sizeof(struct BasicFramingHdr);

// Message passing message types; everything after MSG_REQ_FILE is an ACK
#define MSG_NULL 0
#define MSG_REQ_INIT 1
#define MSG_REQ_REPLICATE 2
#define MSG_REQ_COPY 3
#define MSG_REQ_INDEX 4
#define MSG_REQ_WORK 5
#define MSG_REQ_STATUS 6
#define MSG_REQ_DELETE 7
#define MSG_REQ_TERM 8
#define MSG_REQ_FILE 9
#define MSG_ACK_INIT 11
#define MSG_ACK_REPLICATE 12
#define MSG_ACK_COPY 13
#define MSG_ACK_INDEX 14
#define MSG_ACK_WORK 15
#define MSG_ACK_STATUS 16
#define MSG_ACK_DELETE 17
#define MSG_ACK_TERM 18
#define MSG_ACK_REDIRECT 19
#define MSG_ACK_FILE 20


#endif  /* #ifndef BASICFRAMING_H_ */

