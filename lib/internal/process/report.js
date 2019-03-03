'use strict';
const {
  convertToValidSignal,
  emitExperimentalWarning
} = require('internal/util');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_SYNTHETIC
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const nr = internalBinding('report');
const report = {
  triggerReport(file, err) {
    emitExperimentalWarning('report');

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

    return nr.triggerReport('JavaScript API', 'API', file, err.stack);
  },
  getReport(err) {
    emitExperimentalWarning('report');

    if (err === undefined)
      err = new ERR_SYNTHETIC();
    else if (err === null || typeof err !== 'object')
      throw new ERR_INVALID_ARG_TYPE('err', 'Object', err);

    return nr.getReport(err.stack);
  },
  get directory() {
    emitExperimentalWarning('report');
    return nr.getDirectory();
  },
  set directory(dir) {
    emitExperimentalWarning('report');
    validateString(dir, 'directory');
    return nr.setDirectory(dir);
  },
  get filename() {
    emitExperimentalWarning('report');
    return nr.getFilename();
  },
  set filename(name) {
    emitExperimentalWarning('report');
    validateString(name, 'filename');
    return nr.setFilename(name);
  },
  get signal() {
    emitExperimentalWarning('report');
    return nr.getSignal();
  },
  set signal(sig) {
    emitExperimentalWarning('report');
    validateString(sig, 'signal');
    convertToValidSignal(sig); // Validate that the signal is recognized.
    removeSignalHandler();
    addSignalHandler(sig);
    return nr.setSignal(sig);
  },
  get reportOnFatalError() {
    emitExperimentalWarning('report');
    return nr.shouldReportOnFatalError();
  },
  set reportOnFatalError(trigger) {
    emitExperimentalWarning('report');

    if (typeof trigger !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('trigger', 'boolean', trigger);

    return nr.setReportOnFatalError(trigger);
  },
  get reportOnSignal() {
    emitExperimentalWarning('report');
    return nr.shouldReportOnSignal();
  },
  set reportOnSignal(trigger) {
    emitExperimentalWarning('report');

    if (typeof trigger !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('trigger', 'boolean', trigger);

    nr.setReportOnSignal(trigger);
    removeSignalHandler();
    addSignalHandler();
  },
  get reportOnUncaughtException() {
    emitExperimentalWarning('report');
    return nr.shouldReportOnUncaughtException();
  },
  set reportOnUncaughtException(trigger) {
    emitExperimentalWarning('report');

    if (typeof trigger !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('trigger', 'boolean', trigger);

    return nr.setReportOnUncaughtException(trigger);
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
  nr.triggerReport(sig, 'Signal', null, '');
}

module.exports = {
  addSignalHandler,
  report
};
