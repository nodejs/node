var common = require('../common');
var assert = common.assert;
var CombinedStream = common.CombinedStream;

(function testDataSizeGetter() {
  var combinedStream = CombinedStream.create();

  assert.strictEqual(combinedStream.dataSize, 0);

  // Test one stream
  combinedStream._streams.push({dataSize: 10});
  combinedStream._updateDataSize();
  assert.strictEqual(combinedStream.dataSize, 10);

  // Test two streams
  combinedStream._streams.push({dataSize: 23});
  combinedStream._updateDataSize();
  assert.strictEqual(combinedStream.dataSize, 33);

  // Test currentStream
  combinedStream._currentStream = {dataSize: 20};
  combinedStream._updateDataSize();
  assert.strictEqual(combinedStream.dataSize, 53);

  // Test currentStream without dataSize
  combinedStream._currentStream = {};
  combinedStream._updateDataSize();
  assert.strictEqual(combinedStream.dataSize, 33);

  // Test stream function
  combinedStream._streams.push(function() {});
  combinedStream._updateDataSize();
  assert.strictEqual(combinedStream.dataSize, 33);
})();
