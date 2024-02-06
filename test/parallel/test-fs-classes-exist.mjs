import '../common/index.mjs';
import { equal } from 'node:assert/strict';
import {
  BigIntStats,
  Dir,
  Dirent,
  FSWatcher,
  StatWatcher,
  ReadStream,
  Stats,
  StatFs,
  WriteStream,
} from 'node:fs';

// Test classes listed in https://nodejs.org/api/fs.html#common-objects

equal(typeof BigIntStats, 'function');
equal(typeof Dir, 'function');
equal(typeof Dirent, 'function');
equal(typeof FSWatcher, 'function');
equal(typeof StatWatcher, 'function');
equal(typeof ReadStream, 'function');
equal(typeof Stats, 'function');
equal(typeof StatFs, 'function');
equal(typeof WriteStream, 'function');
