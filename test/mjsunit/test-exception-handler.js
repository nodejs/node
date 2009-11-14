process.mixin(require("./common"));

var MESSAGE = 'catch me if you can';
var caughtException = false;

process.addListener('uncaughtException', function (e) {
  puts("uncaught exception! 1");
  assertEquals(MESSAGE, e.message);
  caughtException = true;
});

process.addListener('uncaughtException', function (e) {
  puts("uncaught exception! 2");
  assertEquals(MESSAGE, e.message);
  caughtException = true;
});

setTimeout(function() {
  throw new Error(MESSAGE);
}, 10);

process.addListener("exit", function () {
  puts("exit");
  assertTrue(caughtException);
});
