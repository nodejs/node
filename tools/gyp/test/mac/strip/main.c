// Copyright (c) 2013 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

static void the_static_function() {}
__attribute__((used)) void the_used_function() {}

__attribute__((visibility("hidden"))) __attribute__((used))
void the_hidden_function() {}
__attribute__((visibility("default"))) __attribute__((used))
void the_visible_function() {}

void the_function() {}

extern const int eci;
__attribute__((used)) int i;
__attribute__((used)) const int ci = 34623;

int main() {
  the_function();
  the_static_function();
  the_used_function();
  the_hidden_function();
  the_visible_function();
}
