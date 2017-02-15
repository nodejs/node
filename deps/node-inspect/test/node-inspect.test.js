'use strict';
const tap = require('tap');

const nodeInspect = require('../');

tap.equal(
  9229,
  nodeInspect.port,
  'Uses the --inspect default port');
