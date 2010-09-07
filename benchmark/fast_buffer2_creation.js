
FastBuffer = require('./fast_buffer2').FastBuffer;
for (var i = 0; i < 1e6; i++) {
  b = new FastBuffer(10);
  b[1] = 2;
}
