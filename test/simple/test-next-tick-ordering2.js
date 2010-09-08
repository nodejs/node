common = require("../common");
assert = common.assert

var order = [];
process.nextTick(function () {
  process.nextTick(function() {
    order.push('nextTick');
  });

  setTimeout(function() {
    order.push('setTimeout');
  }, 0);
})

process.addListener('exit', function () {
  assert.deepEqual(order, ['nextTick', 'setTimeout']);
});
