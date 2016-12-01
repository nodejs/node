'use strict';
require('../common');

// if printing, add to out file

const refreshSizeWrapperStderr = () => {
  console.log('calling stderr._refreshSize');
};

const refreshSizeWrapperStdout = () => {
  console.log('calling stdout._refreshSize');
};

process.stderr._refreshSize = refreshSizeWrapperStderr;
process.stdout._refreshSize = refreshSizeWrapperStdout;

process.emit('SIGWINCH');
