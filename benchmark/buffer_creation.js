SlowBuffer = require('buffer').SlowBuffer;

for (var i = 0; i < 1e6; i++) {
  b = new SlowBuffer(10);
  b[1] = 2
}
