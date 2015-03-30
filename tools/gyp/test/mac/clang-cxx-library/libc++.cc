// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#ifndef _LIBCPP_VERSION
#error expected std library: libc++
#endif

int main() { std::string x; return x.size(); }

