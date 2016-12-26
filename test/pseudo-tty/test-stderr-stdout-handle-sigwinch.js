'use strict';
const common = require('../common');

// if printing, add to out file
const originalRefreshSizeWrapperStderr = process.stderr._refreshSize;
const originalRefreshSizeWrapperStdout = process.stdout._refreshSize;

const refreshSizeWrapperStderr = () => {
  console.log('calling stderr._refreshSize');
  try {
    originalRefreshSizeWrapperStderr.call(process.stderr);
  } catch (e) {
    // EINVAL happens on SmartOS if emulation is incomplete
    if (!common.isSunOS || e.code !== 'EINVAL')
      throw e;
  }
};

const refreshSizeWrapperStdout = () => {
  console.log('calling stdout._refreshSize');
  try {
    originalRefreshSizeWrapperStdout.call(process.stdout);
  } catch (e) {
    // EINVAL happens on SmartOS if emulation is incomplete
    if (!common.isSunOS || e.code !== 'EINVAL')
      throw e;
  }
};

process.stderr._refreshSize = refreshSizeWrapperStderr;
process.stdout._refreshSize = refreshSizeWrapperStdout;

process.emit('SIGWINCH');
