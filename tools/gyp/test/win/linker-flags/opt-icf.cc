// Copyright (c) 2012 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

void similar_function0(char* x) {
  while (*x) {
    ++x;
  }
}

void similar_function1(char* p) {
  while (*p) {
    ++p;
  }
}

void similar_function2(char* q) {
  while (*q) {
    ++q;
  }
}

int main() {
  char* x = "hello";
  similar_function0(x);
  similar_function1(x);
  similar_function2(x);
  return 0;
}
