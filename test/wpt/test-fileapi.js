'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

// FileAPI/blob, FileAPI/file, and FileAPI/reading-data-section have their own
// runners. These two tests live in the FileAPI root.
const included = new Set([
  'fileReader.any.js',
  'FileReaderSync.worker.js',
]);

const runner = new WPTRunner('FileAPI');
runner.setFlags(['--experimental-fileapi']);
for (const spec of runner.specs) {
  if (!included.has(spec.filename)) {
    runner.specs.delete(spec);
  }
}
runner.runJsTests();
