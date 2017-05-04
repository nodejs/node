// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

function MaxSimple(a, b) {
  return a > b ? a : b;
}

function MinSimple(a, b) {
  return a > b ? b : a;
}

%SetForceInlineFlag(MaxSimple);
%SetForceInlineFlag(MinSimple);

// ----------------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.MaxSimple = MaxSimple;
  to.MinSimple = MinSimple;
});

})
