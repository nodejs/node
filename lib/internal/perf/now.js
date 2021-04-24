'use strict';

const {
  Number,
} = primordials;

const {
  timeOriginBigInt,
} = internalBinding('performance');

const ns2ms = 10n ** 6n;
module.exports = function now() {
  const time = process.hrtime.bigint() - timeOriginBigInt;
  return Number(time / ns2ms) + Number(time % ns2ms) / 1e6;
};
