// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

while(true) {
  if (cond) throw Exception();
  console.log("forever");
}

do {
  if (cond) throw Exception();
  console.log("forever");
} while(true);
