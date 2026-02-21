// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//"use strict";

(function () {
  print(arguments);
})();

(function () {
  arguments = 1;
})();

(function () {
  let arguments = 1;
})();

(function () {
  print(undefined);
})();

(function () {
  undefined = 1;
})();

(function () {
  let undefined = 1;
})();

(function () {
  print(eval);
})();

(function () {
  eval = 1;
})();

(function () {
  let eval = 1;
})();
