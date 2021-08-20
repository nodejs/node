'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('FileAPI/blob');

runner.setInitScript(`
  const { Blob } = require('buffer');
  const { ReadableStream } = require('stream/web');
  global.Blob = Blob;
  global.ReadableStream = ReadableStream;
`);

runner.runJsTests();
