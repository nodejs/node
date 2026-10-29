const assert = require('assert');
const path = require('path');

module.exports = async function * customReporter(source) {
  const counters = {};
  for await (const event of source) {
    if (event.data.file) {
      assert.strictEqual(event.data.file, path.resolve(__dirname, '../reporters.js'));
    }
    counters[event.type] = (counters[event.type] ?? 0) + 1;
  }
  yield "custom.js ";
  yield JSON.stringify(counters);
};
