'use strict';

const { hasInspector } = internalBinding('config');
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
  } catch {
    return onError();
  }
}

module.exports = {
  sendInspectorCommand
};
