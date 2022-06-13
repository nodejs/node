// Flags:  --enable-source-maps

'use strict';
require('../common');
const fs = require('fs');

const content = fs.readFileSync(require.resolve('../fixtures/source-map/tabs.js'), 'utf8');
eval(content);
