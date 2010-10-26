console.log("wait...");
var done = 0;
var N = 5000000;
var begin = new Date();
for (var i = 0; i < N; i++) {
  setTimeout(function () {
    if (++done == N) {
      var end = new Date();
      console.log("smaller is better");
      console.log("startup: %d", start - begin);
      console.log("done: %d", end - start);
    }
  }, 1000);
}
var start = new Date();
