// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --print-ast

var x;
`Crashes if OOB read with --print-ast ${x}`;
