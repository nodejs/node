// Copyright (c) 2011 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

extern void input_root();
extern void input_dirname();
extern void input_path();
extern void input_ext();
extern void input_name();

int main() {
  input_root();
  input_dirname();
  input_path();
  input_ext();
  input_name();
  return 0;
}
