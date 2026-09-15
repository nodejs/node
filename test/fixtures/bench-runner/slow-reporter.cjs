'use strict';

const { setTimeout } = require('timers/promises');

module.exports = async function* slowReporter(source) {
  const { promise, resolve } = Promise.withResolvers();
  let emitted = 0;
  const onRecord = () => {
    if (++emitted === source.readableHighWaterMark) resolve();
  };
  for (const type of [
    'bench:plan',
    'bench:start',
    'bench:sample',
    'bench:complete',
    'bench:diagnostic',
    'bench:summary',
  ]) {
    source.on(type, onRecord);
  }
  await promise;

  let samples = 0;
  let stdout = '';
  for await (const record of source) {
    await setTimeout(2);
    if (record.type === 'bench:sample') samples++;
    if (record.type === 'bench:diagnostic' &&
        record.data.stream === 'stdout') {
      stdout += record.data.message;
    }
    if (record.type === 'bench:summary') {
      yield `${JSON.stringify({ samples, stdout })}\n`;
    }
  }
};
