'use strict';

const readline = require('readline');
const { PassThrough } = require('stream');

// Regression test for https://github.com/nodejs/node/issues/61526
// This test ensures that createInterface() doesn't crash when input
// has an onHistoryFileLoaded property (e.g., from a Proxy or jest.mock)

// Test case 1: options object with onHistoryFileLoaded as function
{
  const input = new PassThrough();
  input.onHistoryFileLoaded = () => {};

  readline.createInterface({
    input,
    output: new PassThrough()
  });
}

// Test case 2: options object without onHistoryFileLoaded
{
  const input = new PassThrough();
  readline.createInterface({
    input,
    output: new PassThrough()
  });
}

// Test case 3: options object with onHistoryFileLoaded as non-function
{
  const input = new PassThrough();
  input.onHistoryFileLoaded = { some: 'object' };

  readline.createInterface({
    input,
    output: new PassThrough()
  });
}

// Test case 4: direct stream with onHistoryFileLoaded (original bug scenario)
{
  const input = new PassThrough();
  input.onHistoryFileLoaded = () => {};

  readline.createInterface(input, new PassThrough());
}
