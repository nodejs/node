'use strict';
const fs = require('fs');
const path = require('path');
const v8 = require('v8');

v8.setHeapSnapshotNearHeapLimit(1);
v8.startHeapProfile();
v8.setHeapProfileNearHeapLimit((profile) => {
  fs.writeFileSync('oom.heapprofile', profile);
});

require(path.resolve(__dirname, 'grow.js'));
