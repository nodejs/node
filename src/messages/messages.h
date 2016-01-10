#ifndef SRC_NODE_MESSAGES_API_H_
#define SRC_NODE_MESSAGES_API_H_

// TODO(jasnell): Allow other locales to be swapped in based on build config
#include "en/messages.h"

typedef enum {
#define DEF_NODE_MESSAGES(id,_) MSG_ ## id,
  NODE_MESSAGES(DEF_NODE_MESSAGES)
#undef DEF_NODE_MESSAGES
  MSG_UNKNOWN
} node_messages;

// /**
//  * Returns the const char* message identifier associated with the message key
//  **/
// const char* nodemsgid(int key);
//
// /**
//  * Returns the text of the message associated with the message key
//  **/
// const char* nodemsgstr(int key);
//
// /**
//  * Returns the text of the message with the err key appended
//  **/
// const char * nodeerrstr(int key);
//
#endif  // SRC_NODE_MESSAGES_API_H_
