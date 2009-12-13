sys = require("sys");
assert = require("assert");


var b = new process.Buffer(1024);

sys.puts("b[0] == " + b[0]);
assert.ok(b[0] >= 0);

sys.puts("b[1] == " + b[1]);
assert.ok(b[1] >= 0);

sys.puts("b.length == " + b.length);
assert.equal(1024, b.length);

for (var j = 0; j < 10000; j++) {
  var asciiString = "hello world";

  for (var i = 0; i < asciiString.length; i++) {
    b[i] = asciiString.charCodeAt(i);
  }

  var asciiSlice = b.asciiSlice(0, asciiString.length);

  assert.equal(asciiString, asciiSlice);
}


for (var j = 0; j < 10000; j++) {
  var slice = b.slice(100, 150);
  assert.equal(50, slice.length);
  for (var i = 0; i < 50; i++) {
    assert.equal(b[100+i], slice[i]);
  }
}
