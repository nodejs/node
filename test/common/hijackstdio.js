/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

// Hijack stdout and stderr
const stdWrite = {};
function hijackStdWritable(name, listener) {
  const stream = process[name];
  const _write = stdWrite[name] = stream.write;

  stream.writeTimes = 0;
  stream.write = function(data, callback) {
    try {
      listener(data);
    } catch (e) {
      process.nextTick(() => { throw e; });
    }

    _write.call(stream, data, callback);
    stream.writeTimes++;
  };
}

function restoreWritable(name) {
  process[name].write = stdWrite[name];
  delete process[name].writeTimes;
}

module.exports = {
  hijackStdout: hijackStdWritable.bind(null, 'stdout'),
  hijackStderr: hijackStdWritable.bind(null, 'stderr'),
  restoreStdout: restoreWritable.bind(null, 'stdout'),
  restoreStderr: restoreWritable.bind(null, 'stderr')
};
