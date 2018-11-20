// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var foo = {};
foo.__proto__ = new String("bar");
foo.length = 20;
