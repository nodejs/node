'use strict';

module.exports = async function* destroyingReporter(source) {
  source.once('bench:plan', () => {
    source.destroy(new Error('benchmark reporter closed the stream'));
  });
  yield* source;
};
