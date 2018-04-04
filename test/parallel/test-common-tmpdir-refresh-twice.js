'use strict';

// On CI, running `tmpdir.refresh()` twice fails on the Raspberry Pi devices if
// the tmpdir is not empty for the second call.
//
// This may have something to do with the fact that the tmp directory is NFS
// mounted on those devices.

require('../common');
const tmpdir = require('../common/tmpdir');

const fs = require('fs');
const path = require('path');

tmpdir.refresh();
fs.closeSync(fs.openSync(path.join(tmpdir.path, 'fhqwhgads'), 'w'));
tmpdir.refresh(); // This should not throw.
