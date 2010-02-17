process.mixin(require("./common"));

var got_error = false;

var promise = fs.readdir(fixturesDir);
puts("readdir " + fixturesDir);

promise.addCallback(function (files) {
  p(files);
  assert.deepEqual(['a.js'
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
                   ], files.sort());
});

promise.addErrback(function () {
  puts("error");
  got_error = true;
});

process.addListener("exit", function () {
  assert.equal(false, got_error);
  puts("exit");
});
