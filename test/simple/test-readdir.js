require("../common");
var path = require('path');
var fs = require('fs');

var got_error = false,
    readdirDir = path.join(fixturesDir, "readdir")

var files = ['are'
            , 'dir'
            , 'empty'
            , 'files'
            , 'for'
            , 'just'
            , 'testing.js'
            , 'these'
            ];


console.log('readdirSync ' + readdirDir);
var f = fs.readdirSync(readdirDir);
p(f);
assert.deepEqual(files, f.sort());


console.log("readdir " + readdirDir);
fs.readdir(readdirDir, function (err, f) {
  if (err) {
    console.log("error");
    got_error = true;
  } else {
    p(f);
    assert.deepEqual(files, f.sort());
  }
});

process.addListener("exit", function () {
  assert.equal(false, got_error);
  console.log("exit");
});
