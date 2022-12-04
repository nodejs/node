'use strict';

const inspector = require('inspector');
const { promisify } = require('internal/util');

class Session extends inspector.Session {
  constructor() { super(); } // eslint-disable-line no-useless-constructor
}
Session.prototype.post = promisify(inspector.Session.prototype.post);

module.exports = {
  ...inspector,
  Session,
};
