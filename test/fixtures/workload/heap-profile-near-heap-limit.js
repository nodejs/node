'use strict';
const path = require('path');
const v8 = require('v8');

v8.startHeapProfile();
v8.setHeapProfileNearHeapLimit(1);

require(path.resolve(__dirname, 'grow.js'));
