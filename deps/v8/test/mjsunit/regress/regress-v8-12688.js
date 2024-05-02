// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var yield;
({p: yield} = class {
  q = () => 42;
});

var yield;
({p: yield} = class {
  q = (a) => 42;
});

var yield;
({p: yield} = class {
  q = a => 42;
});

var yield;
({p: yield} = class {
  q = async a => 42;
});

var yield;
({p: yield} = class {
  q = async (a) => 42;
});

var yield;
({p: yield} = class {
  q = async () => 42;
});
