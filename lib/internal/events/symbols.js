'use strict';

const {
  Symbol,
} = primordials;

const kFirstEventParam = Symbol('nodejs.kFirstEventParam');
const kInitialFastPath = Symbol('kInitialFastPath');

module.exports = {
  kFirstEventParam,
  kInitialFastPath,
};
