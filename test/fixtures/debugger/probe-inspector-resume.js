'use strict';
const { Session } = require('inspector');

function callInspectorResume() {
  const session = new Session();
  session.connect();
  // Use a callback to ensure the probing resume happens after this one is delivered.
  session.post('Debugger.resume', () => {});
  return 1;
}

const probeLine = 1;
module.exports = { callInspectorResume };
setInterval(() => {}, 1000);  // Keep it alive to prevent early exit.
