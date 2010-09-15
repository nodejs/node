var assert = require('assert');

var knownGlobals = [ setTimeout
                   , setInterval
                   , clearTimeout
                   , clearInterval
                   , console
                   , Buffer
                   , process
                   , global
                   , __module
                   , include
                   , puts
                   , print
                   , p
                   ];

for (var x in global) {
  var found = false;

  for (var y in knownGlobals) {
    if (global[x] === knownGlobals[y]) {
      found = true;
      break;
    }
  }

  if (!found) {
    console.error("Unknown global: %s", x);
    assert.ok(false);
  }
}
