require('../common');
path = require('path');
childProccess = require('child_process');
fs = require('fs');
scriptString = path.join(fixturesDir, 'print-chars.js');
scriptBuffer = path.join(fixturesDir, 'print-chars-from-buffer.js');
tmpFile = path.join(fixturesDir, 'stdout.txt');

function test (size, useBuffer, cb) {
  var cmd = process.argv[0]
          + ' '
          + (useBuffer ? scriptBuffer : scriptString)
          + ' '
          + size
          + ' > '
          + tmpFile
          ;

  try {
    fs.unlinkSync(tmpFile);
  } catch (e) {}

  print(size + ' chars to ' + tmpFile + '...');

  childProccess.exec(cmd, function(err) {
    if (err) throw err;

    console.log('done!');

    var stat = fs.statSync(tmpFile);

    console.log(tmpFile + ' has ' + stat.size + ' bytes');

    assert.equal(size, stat.size);
    fs.unlinkSync(tmpFile);

    cb();
  });
}

finished = false;
test(1024*1024, false, function () {
  console.log("Done printing with string");
  test(1024*1024, true, function () {
    console.log("Done printing with buffer");
    finished = true;
  });
});

process.addListener('exit', function () {
  assert.ok(finished);
});
