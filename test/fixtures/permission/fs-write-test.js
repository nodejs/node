const fs = require('fs');
const path = require('path');
const os = require('os');

const tmpFile = path.join(os.tmpdir(), 'permission-test-' + Date.now() + '.txt');
fs.writeFileSync(tmpFile, 'test');
