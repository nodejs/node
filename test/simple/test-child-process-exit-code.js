require("../common");
spawn = require('child_process').spawn,
path = require('path');

exits = 0;

exitScript = path.join(fixturesDir, 'exit.js')
exitChild = spawn(process.argv[0], [exitScript, 23]);
exitChild.addListener('exit', function(code, signal) {
  assert.strictEqual(code, 23);
  assert.strictEqual(signal, null);

  exits++;
});



errorScript = path.join(fixturesDir, 'child_process_should_emit_error.js')
errorChild = spawn(process.argv[0], [errorScript]);
errorChild.addListener('exit', function(code, signal) {
  assert.ok(code !== 0);
  assert.strictEqual(signal, null);

  exits++;
});


process.addListener('exit', function () {
  assert.equal(2, exits);
});
