'use strict';
// This tests that when there is a burst of fs watch events, the events
// emitted after the consumer receives the initial event and before the
// control returns back to fs.watch() can be queued up and show up
// in the next iteration.
const common = require('../common');
const { watch, writeFile } = require('fs/promises');
const fs = require('fs');
const assert = require('assert');
const { join } = require('path');
const { setTimeout } = require('timers/promises');
const tmpdir = require('../common/tmpdir');

class WatchTestCase {
  constructor(dirName, files) {
    this.dirName = dirName;
    this.files = files;
  }
  get dirPath() { return tmpdir.resolve(this.dirName); }
  filePath(fileName) { return join(this.dirPath, fileName); }

  async run() {
    await Promise.all([this.watchFiles(), this.writeFiles()]);
    assert(!this.files.length);
  }
  async watchFiles() {
    const watcher = watch(this.dirPath);
    for await (const evt of watcher) {
      const idx = this.files.indexOf(evt.filename);
      if (idx < 0) continue;
      this.files.splice(idx, 1);
      await setTimeout(common.platformTimeout(100));
      if (!this.files.length) break;
    }
  }
  async writeFiles() {
    for (const fileName of [...this.files]) {
      await writeFile(this.filePath(fileName), Date.now() + fileName.repeat(1e4));
    }
    await setTimeout(common.platformTimeout(100));
  }
}

const kCases = [
  // Watch on a directory should callback with a filename on supported systems
  new WatchTestCase(
    'watch1',
    ['foo', 'bar', 'baz']
  ),
];

tmpdir.refresh();

for (const testCase of kCases) {
  fs.mkdirSync(testCase.dirPath);
  testCase.run().then(common.mustCall());
}
