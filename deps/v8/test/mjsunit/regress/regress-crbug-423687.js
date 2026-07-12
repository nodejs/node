// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var json = '{"a":{"c":2.1,"d":0},"b":{"c":7,"1024":8}}';
var data = JSON.parse(json);

data.b.c++;
