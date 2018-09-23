// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt

// The rightmost cons string is created first, resulting in an empty left part.
eval(" " + ("" + "try {;} catch (_) {}"));
