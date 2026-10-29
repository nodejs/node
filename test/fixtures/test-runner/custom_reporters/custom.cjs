const { Transform } = require('node:stream');

const customReporter = new Transform({
  writableObjectMode: true,
  transform(event, encoding, callback) {
    this.counters ??= {};
    this.counters[event.type] = (this.counters[event.type] ?? 0) + 1;
    callback();
  },
  flush(callback) {
    this.push('custom.cjs ')
    this.push(JSON.stringify(this.counters));
    callback();
  }
});

module.exports = customReporter;
