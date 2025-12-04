// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertThrows("/*for..in*/for(var [x5, functional] = this = function(id) { return id } in false) var x2, x;", SyntaxError);
