require("../common");
var spawn = require('child_process').spawn
  , path = require('path')
  , sub = path.join(fixturesDir, 'exit.js')
  , child = spawn(process.argv[0], [sub, 23])
  ; 

child.addListener('exit', function(code, signal) {
  assert.strictEqual(code, 23);
  assert.strictEqual(signal, null);
});