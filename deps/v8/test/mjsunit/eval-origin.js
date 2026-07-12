// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

Error.prepareStackTrace = function(exception, frames) {
  return frames[0].getEvalOrigin();
}

var sc = 0;
// TODO(4921)
Object.defineProperty(globalThis, "source", {get() { return `new Error(${sc++})` }});
var eval_origin;
var geval = eval;
var log = [];

(function() {
  log.push([geval(source).stack, "19:13"]);
  log.push([geval(source).stack, "20:13"]);
  log.push([geval(source).stack, "21:13"]);
})();

(function() {
  log.push([eval(source).stack, "25:13"]);
  log.push([eval(source).stack, "26:13"]);
  log.push([eval(source).stack, "27:13"]);
})();

log.push([eval(source).stack, "30:11"]);
log.push([eval(source).stack, "31:11"]);
log.push([eval(source).stack, "32:11"]);

Error.prepareStackTrace = undefined;

for (var item of log) {
  var stacktraceline = item[0];
  var expectation = item[1];
  var re = new RegExp(`:${expectation}\\)$`);
  assertTrue(re.test(stacktraceline));
}
