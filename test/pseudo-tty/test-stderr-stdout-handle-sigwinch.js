'use strict';
require('../common');

// if printing, add to out file
const originalRefreshSizeWrapperStderr = process.stderr._refreshSize;
const originalRefreshSizeWrapperStdout = process.stdout._refreshSize;

const refreshSizeWrapperStderr = () => {
  console.log('calling stderr._refreshSize');
  originalRefreshSizeWrapperStderr.call(process.stderr);
};

const refreshSizeWrapperStdout = () => {
  console.log('calling stdout._refreshSize');
  originalRefreshSizeWrapperStdout.call(process.stdout);
};

process.stderr._refreshSize = refreshSizeWrapperStderr;
process.stdout._refreshSize = refreshSizeWrapperStdout;

process.emit('SIGWINCH');
