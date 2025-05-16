// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const wsUrl = 'ws://nodejs.org';

const ws = new WebSocket(wsUrl);

ws.addEventListener('open', common.mustNotCall('WebSocket connection should be blocked'));
// WebSocket implementation doesn't expose the Node.js specific errors
// so ERR_ACCESS_DENIED won't be "caught" explicitly.
// For now, let's just assert the error
// TODO(rafaelgss): make this test more comprehensive
ws.addEventListener('error', common.mustCall());
