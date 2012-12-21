// Copyright (c) 2011 Google Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

static void the_static_function() {}

void the_function() {
  the_static_function();
}
