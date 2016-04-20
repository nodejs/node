'use strict';
require('../common');
var assert = require('assert');

var stream = require('stream');
var state = 0;

/*
What you do
var stream = new tream.Transform({
  transform: function transformCallback(chunk, _, next) {
    // part 1
    this.push(chunk);
    //part 2
    next();
  },
  end: function endCallback(done) {
    // part 1
    process.nextTick(function () {
      // part 2
      done();
    });
  },
  flush: function flushCallback(done) {
    // part 1
    process.nextTick(function () {
      // part 2
      done();
    });
  }
});
t.on('data', dataListener);
t.on('end', endListener);
t.on('finish', finishListener);
t.write(1);
t.write(4);
t.end(7, endMethodCallback);

The order things are called

1. transformCallback part 1
2. dataListener
3. transformCallback part 2
4. transformCallback part 1
5. dataListener
6. transformCallback part 2
7. transformCallback part 1
8. dataListener
9. transformCallback part 2
10. endCallback part 1
11. endCallback part 2
12. flushCallback part 1
13. finishListener
14. endMethodCallback
15. flushCallback part 2
16. endListener
*/

var t = new stream.Transform({
  objectMode: true,
  transform: function(chunk, _, next) {
    assert.equal(++state, chunk, 'transformCallback part 1');
    this.push(state);
    assert.equal(++state, chunk + 2, 'transformCallback part 2');
    process.nextTick(next);
  },
  end: function(done) {
    state++;
    assert.equal(state, 10, 'endCallback part 1');
    setTimeout(function() {
      state++;
      assert.equal(state, 11, 'endCallback part 2');
      done();
    }, 100);
  },
  flush: function(done) {
    state++;
    assert.equal(state, 12, 'flushCallback part 1');
    process.nextTick(function() {
      state++;
      assert.equal(state, 15, 'flushCallback part 2');
      done();
    });
  }
});
t.on('finish', function() {
  state++;
  assert.equal(state, 13, 'finishListener');
});
t.on('end', function() {
  state++;
  assert.equal(state, 16, 'end event');
});
t.on('data', function(d) {
  assert.equal(++state, d + 1, 'dataListener');
});
t.write(1);
t.write(4);
t.end(7, function() {
  state++;
  assert.equal(state, 14, 'endMethodCallback');
});
