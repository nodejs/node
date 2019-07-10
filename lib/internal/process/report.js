'use strict';
const {
  ERR_INVALID_ARG_TYPE,
  ERR_SYNTHETIC
} = require('internal/errors').codes;
const { validateSignalName, validateString } = require('internal/validators');
const nr = internalBinding('report');
const { JSON } = primordials;
const report = {
  writeReport(file, err) {
    if (typeof file === 'object' && file !== null) {
      err = file;
      file = undefined;
    } else if (file !== undefined && typeof file !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('file', 'String', file);
    } else if (err === undefined) {
      err = new ERR_SYNTHETIC();
    } else if (err === null || typeof err !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('err', 'Object', err);
    }

    return nr.writeReport('JavaScript API', 'API', file, err.stack);
  },
  getReport(err) {
    if (err === undefined)
      err = new ERR_SYNTHETIC();
    else if (err === null || typeof err !== 'object')
      throw new ERR_INVALID_ARG_TYPE('err', 'Object', err);

    return JSON.parse(nr.getReport(err.stack));
  },
  get directory() {
    return nr.getDirectory();
  },
  set directory(dir) {
    validateString(dir, 'directory');
    nr.setDirectory(dir);
  },
  get filename() {
    return nr.getFilename();
  },
  set filename(name) {
    validateString(name, 'filename');
    nr.setFilename(name);
  },
  get signal() {
    return nr.getSignal();
  },
  set signal(sig) {
    validateSignalName(sig, 'signal');
    removeSignalHandler();
    addSignalHandler(sig);
    nr.setSignal(sig);
  },
  get reportOnFatalError() {
    return nr.shouldReportOnFatalError();
  },
  set reportOnFatalError(trigger) {
    if (typeof trigger !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('trigger', 'boolean', trigger);

    nr.setReportOnFatalError(trigger);
  },
  get reportOnSignal() {
    return nr.shouldReportOnSignal();
  },
  set reportOnSignal(trigger) {
    if (typeof trigger !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('trigger', 'boolean', trigger);

    nr.setReportOnSignal(trigger);
    removeSignalHandler();
    addSignalHandler();
  },
  get reportOnUncaughtException() {
    return nr.shouldReportOnUncaughtException();
  },
  set reportOnUncaughtException(trigger) {
    if (typeof trigger !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('trigger', 'boolean', trigger);

    nr.setReportOnUncaughtException(trigger);
  }
};

function addSignalHandler(sig) {
  if (nr.shouldReportOnSignal()) {
    if (typeof sig !== 'string')
      sig = nr.getSignal();

    process.on(sig, signalHandler);
  }
}

function removeSignalHandler() {
  const sig = nr.getSignal();

  if (sig)
    process.removeListener(sig, signalHandler);
}

function signalHandler(sig) {
  nr.writeReport(sig, 'Signal', null, '');
}

module.exports = {
  addSignalHandler,
  report
};
