process.mixin(require("./common"));

var got_error = false;

fs.readdir(fixturesDir, function (err, files) {
  if (err) {
    puts("error");
    got_error = true;
  } else {
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
  }
});
puts("readdir " + fixturesDir);

process.addListener("exit", function () {
  assert.equal(false, got_error);
  puts("exit");
});
