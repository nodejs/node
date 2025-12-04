// Flags: --permission --allow-net --allow-fs-read=*
'use strict';

require('../common');
const assert = require('node:assert');

{
  assert.ok(process.permission.has('net'));
}
