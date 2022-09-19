'use strict';
const path = require('path');
const v8 = require('v8');

v8.setHeapSnapshotNearHeapLimit(+process.env.limit);
if (process.env.limit2) {
  v8.setHeapSnapshotNearHeapLimit(+process.env.limit2);
}
require(path.resolve(__dirname, 'grow.js'));
