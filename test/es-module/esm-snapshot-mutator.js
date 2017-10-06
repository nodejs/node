/* eslint-disable required-modules */
'use strict';
const shouldSnapshotFilePath = require.resolve('./esm-snapshot.js');
require('./esm-snapshot.js');
require.cache[shouldSnapshotFilePath].exports++;
