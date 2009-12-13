sys = require("sys");
assert = require("assert");


var b = new process.Buffer(1024);

sys.puts("b.length == " + b.length);
assert.equal(1024, b.length);

for (var i = 0; i < 1024; i++) {
  assert.ok(b[i] >= 0);
  b[i] = i % 256;
}

for (var i = 0; i < 1024; i++) {
  assert.equal(i % 256, b[i]);
}

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
