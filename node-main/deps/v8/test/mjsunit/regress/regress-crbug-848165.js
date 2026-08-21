// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class cls0 {
  static get length(){ return 42; };
  static get [1](){ return 21; };
};
Object.defineProperty(cls0, "length", {value:'1'});
