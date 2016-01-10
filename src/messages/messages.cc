#include "messages.h"

// const char* nodemsgid(int key) {
//   switch(key) {
// #define ID_NODE_MESSAGES(id, _) case SRC_MSG_ ## id: return #id;
//     NODE_SRC_MESSAGES(ID_NODE_MESSAGES)
// #undef ID_NODE_MESSAGES
//     default: return NODE_MESSAGE_UNKNOWN;
//   }
// }
//
// const char* nodemsgstr(int key) {
//   switch(key) {
// #define STR_NODE_MESSAGES(id, msg) case SRC_MSG_ ## id: return msg;
//     NODE_SRC_MESSAGES(STR_NODE_MESSAGES)
// #undef STR_NODE_MESSAGES
//     default: return NODE_MESSAGE_UNKNOWN;
//   }
// }
//
// const char* nodeerrstr(int key) {
//   switch(key) {
// #define STR_NODE_MESSAGES(id, msg) case SRC_MSG_ ## id: return msg " (" #id ")";
//     NODE_SRC_MESSAGES(STR_NODE_MESSAGES)
// #undef STR_NODE_MESSAGES
//     default: return NODE_MESSAGE_UNKNOWN;
//   }
// }
