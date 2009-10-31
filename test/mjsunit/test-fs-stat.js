process.mixin(require("./common"));

var got_error = false;
var success_count = 0;
var stats;

var promise = posix.stat(".");

promise.addCallback(function (_stats) {
  stats = _stats;
  p(stats);
  success_count++;
});

promise.addErrback(function () {
  got_error = true;
});

puts("stating: " + __filename);
posix.stat(__filename).addCallback(function (s) {
  p(s);
  success_count++;

  puts("isDirectory: " + JSON.stringify( s.isDirectory() ) );
  assertFalse(s.isDirectory());

  puts("isFile: " + JSON.stringify( s.isFile() ) );
  assertTrue(s.isFile());

  puts("isSocket: " + JSON.stringify( s.isSocket() ) );
  assertFalse(s.isSocket());

  puts("isBlockDevice: " + JSON.stringify( s.isBlockDevice() ) );
  assertFalse(s.isBlockDevice());

  puts("isCharacterDevice: " + JSON.stringify( s.isCharacterDevice() ) );
  assertFalse(s.isCharacterDevice());

  puts("isFIFO: " + JSON.stringify( s.isFIFO() ) );
  assertFalse(s.isFIFO());

  puts("isSymbolicLink: " + JSON.stringify( s.isSymbolicLink() ) );
  assertFalse(s.isSymbolicLink());
}).addErrback(function () {
  got_error = true;
});


process.addListener("exit", function () {
  assertEquals(2, success_count);
  assertFalse(got_error);
  assertTrue(stats.mtime instanceof Date);
});

