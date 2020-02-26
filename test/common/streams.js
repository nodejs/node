/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

async function collectStream(readable, callback) {
  let data = '';
  try {
    readable.setEncoding('utf8');
    for await (const chunk of readable)
      data += chunk;
    if (typeof callback === 'function')
      process.nextTick(callback, null, data);
    return data;
  } catch (err) {
    if (typeof callback === 'function') {
      process.nextTick(callback, err);
    } else {
      throw err;
    }
  }
}

function collectChildStreams(child, waitFor) {
  const stdout = child.stdout && collectStream(child.stdout);
  const stderr = child.stderr && collectStream(child.stderr);
  return Promise.all([stdout, stderr, waitFor])
    .then(([stdout, stderr, data]) => ({ stdout, stderr, data }));
}

module.exports = {
  collectChildStreams,
  collectStream,
};
