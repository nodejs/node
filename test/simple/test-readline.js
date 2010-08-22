common = require("../common");
assert = common.assert;
var readline = require("readline");

var key = {
  xterm: {
    home: [27, 91, 72],
    end: [27, 91, 70]
  },
  gnome: {
    home: [27, 79, 72],
    end: [27, 79, 70]
  },
  rxvt: {
    home: [27, 91, 55],
    end: [27, 91, 56]
  }
};

var fakestream = {
  fd: 1,
  write: function(bytes) {
  }
};

var rl = readline.createInterface(fakestream, function (text) {
  return [[], ""];
});

rl.write('foo');
assert.equal(3, rl.cursor);
rl.write(key.xterm.home);
assert.equal(0, rl.cursor);
rl.write(key.xterm.end);
assert.equal(3, rl.cursor);
rl.write(key.rxvt.home);
assert.equal(0, rl.cursor);
rl.write(key.rxvt.end);
assert.equal(3, rl.cursor);
rl.write(key.gnome.home);
assert.equal(0, rl.cursor);
rl.write(key.gnome.end);
assert.equal(3, rl.cursor);
