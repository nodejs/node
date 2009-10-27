// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef SRC_DNS_H_
#define SRC_DNS_H_

#include <node.h>
#include <v8.h>

namespace node {

class DNS {
 public:
  static void Initialize(v8::Handle<v8::Object> target);
};

}  // namespace node
#endif  // SRC_DNS_H_
