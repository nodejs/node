process.mixin(require("./common"));

var got_error = false;
var success_count = 0;

var __file = path.join(fixturesDir, "a.js");

var promise = fs.chmod(__file, 0777);

promise.addCallback(function () {
  puts(fs.statSync(__file).mode);
  assert.equal("777", (fs.statSync(__file).mode & 0777).toString(8));
  
  fs.chmodSync(__file, 0644);
  assert.equal("644", (fs.statSync(__file).mode & 0777).toString(8));
  success_count++;
});

promise.addErrback(function () {
  got_error = true;
});

process.addListener("exit", function () {
  assert.equal(1, success_count);
  assert.equal(false, got_error);
});

