// Flags: --experimental-workers
'use strict';

var common = require('../common');

var tests = [
  'test/parallel/test-domain-implicit-fs.js',
  'test/parallel/test-fs-access.js',
  'test/parallel/test-fs-append-file.js',
  'test/parallel/test-fs-append-file-sync.js',
  'test/parallel/test-fs-chmod.js',
  'test/parallel/test-fs-empty-readStream.js',
  'test/parallel/test-fs-error-messages.js',
  'test/parallel/test-fs-exists.js',
  'test/parallel/test-fs-fsync.js',
  'test/parallel/test-fs-long-path.js',
  'test/parallel/test-fs-make-callback.js',
  'test/parallel/test-fs-mkdir.js',
  'test/parallel/test-fs-non-number-arguments-throw.js',
  'test/parallel/test-fs-null-bytes.js',
  'test/parallel/test-fs-open-flags.js',
  'test/parallel/test-fs-open.js',
  'test/parallel/test-fs-read-buffer.js',
  'test/parallel/test-fs-readfile-empty.js',
  'test/parallel/test-fs-readfile-error.js',
  'test/parallel/test-fs-readfile-pipe.js',
  'test/parallel/test-fs-read-file-sync-hostname.js',
  'test/parallel/test-fs-read-file-sync.js',
  'test/parallel/test-fs-readfile-unlink.js',
  'test/parallel/test-fs-readfile-zero-byte-liar.js',
  'test/parallel/test-fs-read.js',
  'test/parallel/test-fs-read-stream-err.js',
  'test/parallel/test-fs-read-stream-fd.js',
  'test/parallel/test-fs-read-stream-fd-leak.js',
  'test/parallel/test-fs-read-stream-inherit.js',
  'test/parallel/test-fs-read-stream.js',
  'test/parallel/test-fs-read-stream-resume.js',
  // Workers cannot chdir.
  // 'test/parallel/test-fs-realpath.js',
  'test/parallel/test-fs-sir-writes-alot.js',
  'test/parallel/test-fs-stat.js',
  'test/parallel/test-fs-stream-double-close.js',
  'test/parallel/test-fs-symlink-dir-junction.js',
  'test/parallel/test-fs-symlink-dir-junction-relative.js',
  'test/parallel/test-fs-symlink.js',
  'test/parallel/test-fs-sync-fd-leak.js',
  'test/parallel/test-fs-truncate-fd.js',
  'test/parallel/test-fs-truncate-GH-6233.js',
  'test/parallel/test-fs-truncate.js',
  'test/parallel/test-fs-utimes.js',
  'test/parallel/test-fs-write-buffer.js',
  'test/parallel/test-fs-write-file-buffer.js',
  'test/parallel/test-fs-write-file.js',
  // Workers cannot change umask.
  // 'test/parallel/test-fs-write-file-sync.js',
  'test/parallel/test-fs-write.js',
  'test/parallel/test-fs-write-stream-change-open.js',
  'test/parallel/test-fs-write-stream-end.js',
  'test/parallel/test-fs-write-stream-err.js',
  'test/parallel/test-fs-write-stream.js',
  'test/parallel/test-fs-write-string-coerce.js',
  'test/parallel/test-fs-write-sync.js'
];

var parallelism = 4;
var testsPerThread = Math.ceil(tests.length / parallelism);
for (var i = 0; i < parallelism; ++i) {
  (function(i) {
    var shareOfTests =
        tests.slice(i * testsPerThread, (i + 1) * testsPerThread);
    var cur = Promise.resolve();
    shareOfTests.forEach(function(testFile) {
      cur = cur.then(function() {
        return common.runTestInsideWorker(testFile, {testThreadId: '' + i});
      });
    });
  })(i);
}
