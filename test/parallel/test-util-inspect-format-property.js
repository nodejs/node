'use strict';

require('../common');

const { inspect } = require('util');

class CustomError extends Error {
  get cause() {
    return this._cause;
  }
  get errors() {
    return this._errors;
  }
  constructor(_cause, _errors) {
    super();
    this._cause = _cause;
    this._errors = _errors;
  }
}

// "cause" and "errors" properties should not
//  cause to a error to be thrown while formatting
inspect(new CustomError('', []));
