// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $iteratorPrototype;

(function(global, utils) {
  "use strict";
  %CheckIsBootstrapping();

  var GlobalObject = global.Object;

  // 25.1.2.1 %IteratorPrototype% [ @@iterator ] ( )
  function IteratorPrototypeIterator() {
    return this;
  }

  utils.SetFunctionName(IteratorPrototypeIterator, symbolIterator);
  %AddNamedProperty($iteratorPrototype, symbolIterator,
      IteratorPrototypeIterator, DONT_ENUM);
})
