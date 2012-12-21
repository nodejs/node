// Copyright (c) 2011 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
namespace gen {
  extern void c();
  extern void baz();
}

int main() {
  gen::c();
  gen::baz();
}
