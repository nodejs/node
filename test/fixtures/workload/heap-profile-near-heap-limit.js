'use strict';
const fs = require('fs');
const path = require('path');
const v8 = require('v8');

v8.startHeapProfile();
v8.setHeapProfileNearHeapLimit((profile) => {
  fs.writeFileSync('oom.heapprofile', profile);
  process.exit(0);
});

require(path.resolve(__dirname, 'grow.js'));
