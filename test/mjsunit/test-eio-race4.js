process.mixin(require("./common"));

var N = 100;
var j = 0;

for (var i = 0; i < N; i++) {
  posix.stat("does-not-exist-" + i) // these files don't exist
  .addErrback(function (e) {
    j++; // only makes it to about 17
    puts("finish " + j);
  })
  .addCallback(function () {
    puts("won't be called");
  });
}

process.addListener("exit", function () {
  assert.equal(N, j);
});
