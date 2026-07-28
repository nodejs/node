'use strict';

function neverCalled() {
  return 1;
}

const reached = 1;
throw new Error('boom');
neverCalled();
