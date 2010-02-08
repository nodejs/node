process.mixin(require("./common"));

var sub = path.join(fixturesDir, 'print-chars.js');

completedTests = 0;

function test (n, cb) {
  var child = process.createChildProcess(process.argv[0], [sub, n]);

  var count = 0;

  child.addListener("error", function (data){
    if (data) {
      puts("parent stderr: " + data);
      assert.ok(false);
    }
  });

  child.addListener("output", function (data){
    if (data) {
      count += data.length;
    }
  });

  child.addListener("exit", function (data) {
    assert.equal(n, count);
    puts(n + " okay");
    completedTests++;
    if (cb) cb();
  });
}



test(5000, function () {
  test(50000, function () {
    test(500000);
  });
});


process.addListener('exit', function () {
  assert.equal(3, completedTests);
});
