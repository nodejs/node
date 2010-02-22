process.mixin(require("./common"));

var got_error = false;

var files = ['a.js'
            , 'b'
            , 'cycles'
            , 'echo.js'
            , 'multipart.js'
            , 'nested-index'
            , 'print-chars.js'
            , 'test_ca.pem'
            , 'test_cert.pem'
            , 'test_key.pem'
            , 'throws_error.js'
            , 'x.txt'
            ];


puts('readdirSync ' + fixturesDir);
var f = fs.readdirSync(fixturesDir);
p(f);
assert.deepEqual(files, f.sort());


puts("readdir " + fixturesDir);
fs.readdir(fixturesDir, function (err, f) {
  if (err) {
    puts("error");
    got_error = true;
  } else {
    p(f);
    assert.deepEqual(files, f.sort());
  }
});

process.addListener("exit", function () {
  assert.equal(false, got_error);
  puts("exit");
});
