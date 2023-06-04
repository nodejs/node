"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = exports.PARAM_YIELD = exports.PARAM_RETURN = exports.PARAM_IN = exports.PARAM_AWAIT = exports.PARAM = void 0;
exports.functionFlags = functionFlags;
const PARAM = 0b0000,
  PARAM_YIELD = 0b0001,
  PARAM_AWAIT = 0b0010,
  PARAM_RETURN = 0b0100,
  PARAM_IN = 0b1000;
exports.PARAM_IN = PARAM_IN;
exports.PARAM_RETURN = PARAM_RETURN;
exports.PARAM_AWAIT = PARAM_AWAIT;
exports.PARAM_YIELD = PARAM_YIELD;
exports.PARAM = PARAM;
class ProductionParameterHandler {
  constructor() {
    this.stacks = [];
  }
  enter(flags) {
    this.stacks.push(flags);
  }
  exit() {
    this.stacks.pop();
  }
  currentFlags() {
    return this.stacks[this.stacks.length - 1];
  }
  get hasAwait() {
    return (this.currentFlags() & PARAM_AWAIT) > 0;
  }
  get hasYield() {
    return (this.currentFlags() & PARAM_YIELD) > 0;
  }
  get hasReturn() {
    return (this.currentFlags() & PARAM_RETURN) > 0;
  }
  get hasIn() {
    return (this.currentFlags() & PARAM_IN) > 0;
  }
}
exports.default = ProductionParameterHandler;
function functionFlags(isAsync, isGenerator) {
  return (isAsync ? PARAM_AWAIT : 0) | (isGenerator ? PARAM_YIELD : 0);
}

//# sourceMappingURL=production-parameter.js.map
