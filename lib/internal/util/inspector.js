'use strict';

const hasInspector = process.config.variables.v8_enable_inspector === 1;
const inspector = hasInspector ? require('inspector') : undefined;

let session;

function sendInspectorCommand(cb, onError) {
  if (!hasInspector) return onError();
  if (session === undefined) session = new inspector.Session();
  try {
    session.connect();
    try {
      return cb(session);
    } finally {
      session.disconnect();
    }
  } catch (e) {
    return onError();
  }
}

module.exports = {
  sendInspectorCommand
};
