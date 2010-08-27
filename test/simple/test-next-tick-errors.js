common = require("../common");
assert = common.assert

var order = [],
    exceptionHandled = false;

// This nextTick function will throw an error.  It should only be called once.
// When it throws an error, it should still get removed from the queue.
process.nextTick(function() {
  order.push('A');
  // cause an error
  what();
});

// This nextTick function should remain in the queue when the first one
// is removed.
process.nextTick(function() {
  order.push('C');
});

process.addListener('uncaughtException', function() {
  if (!exceptionHandled) {
    exceptionHandled = true;
    order.push('B');
    // We call process.nextTick here to make sure the nextTick queue is
    // processed again. If everything goes according to plan then when the queue
    // gets ran there will be two functions with this being the second.
    process.nextTick(function() {
      order.push('D');
    });
  }
  else {
    // If we get here then the first process.nextTick got called twice
    order.push('OOPS!');
  }
});

process.addListener('exit', function() {
  assert.deepEqual(['A','B','C','D'], order);
});

