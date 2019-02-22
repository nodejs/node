'use strict';
require('../common');
throw {  // eslint-disable-line no-throw-literal
  get stack() {
    throw new Error('weird throw but ok');
  },
  get name() {
    throw new Error('weird throw but ok');
  },
};
