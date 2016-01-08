#include "messages.h"

const char* node_message_id(int key) {
  switch(key) {
#define ID_NODE_MESSAGES(id, _) case MSG_ ## id: return #id;
    NODE_MESSAGES(ID_NODE_MESSAGES)
#undef ID_NODE_MESSAGES
    default: return NODE_MESSAGE_UNKNOWN;
  }
}

const char* node_message_str(int key) {
  switch(key) {
#define STR_NODE_MESSAGES(id, msg) case MSG_ ## id: return msg;
    NODE_MESSAGES(STR_NODE_MESSAGES)
#undef STR_NODE_MESSAGES
    default: return NODE_MESSAGE_UNKNOWN;
  }
}
