'use strict';
require('../common');

const path = require('path');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const { NodeInstance } = require('../common/inspector-helper.js');

// `sourceMapCacheToObject` and `appendCJSCache` methods of
// /lib/internal/source_map/source_map_cache.js
// should be called after process shutdown with flags
// '--inspect' '--enable-source-maps' end env NODE_V8_COVERAGE
new NodeInstance(
  ['--inspect', '--enable-source-maps'],
  '',
  require.resolve('../fixtures/source-map/dummy.js'),
  {
    env: {
      ...process.env,
      NODE_V8_COVERAGE: path.join(tmpdir.path, `source_map_${__filename}`)
    }
  }
);
