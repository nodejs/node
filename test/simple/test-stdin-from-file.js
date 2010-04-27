require('../common');
var TEST_STR = "abc\n123\nhello world\nsomething else"
  , path = require('path')
  , childProccess = require('child_process')
  , fs = require('fs')
  , stdoutScript = path.join(fixturesDir, 'echo.js')
  , tmpFile = path.join(fixturesDir, 'stdin.txt')
  , cmd = process.argv[0] + ' ' + stdoutScript + ' < ' + tmpFile
  ;

puts(cmd + "\n\n");

try {
  fs.unlinkSync(tmpFile);
} catch (e) {}

fs.writeFileSync(tmpFile, TEST_STR);

childProccess.exec(cmd, function(err, stdout, stderr) {
  fs.unlinkSync(tmpFile);

  if (err) throw err;
  puts(stdout);
  assert.equal(stdout, "hello world\r\n" + TEST_STR);
  assert.equal("", stderr);
});
