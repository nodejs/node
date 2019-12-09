'use strict';

const credentials = internalBinding('credentials');

if (credentials.implementsPosixCredentials) {
  // TODO: this should be detached from ERR_WORKER_UNSUPPORTED_OPERATION
  const { unavailable } = require('internal/process/worker_thread_only');

  process.initgroups = unavailable('process.initgroups()');
  process.setgroups = unavailable('process.setgroups()');
  process.setegid = unavailable('process.setegid()');
  process.seteuid = unavailable('process.seteuid()');
  process.setgid = unavailable('process.setgid()');
  process.setuid = unavailable('process.setuid()');
}

// ---- keep the attachment of the wrappers above so that it's easier to ----
// ----              compare the setups side-by-side                    -----
