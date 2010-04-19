require('../common');
var path = require('path')
  , childProccess = require('child_process')
  , fs = require('fs')
  , stdoutScript = path.join(path.dirname(__dirname), 'fixtures/stdout.js')
  , tmpFile = path.join(path.dirname(__dirname), 'fixtures/stdout.txt')
  , cmd = process.argv[0]+' '+stdoutScript+' > '+tmpFile;

try {
  fs.unlinkSync(tmpFile);
} catch (e) {}

childProccess.exec(cmd, function(err) {
  if (err) throw err;

  var data = fs.readFileSync(tmpFile);
  assert.equal(data, "test\n");
  fs.unlinkSync(tmpFile);
});