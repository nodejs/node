// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <winsock2.h>

int main() {
  WSAStartup(0, 0);
  return 0;
}
