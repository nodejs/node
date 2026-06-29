'use strict';

const inspector = require('inspector');
const { promisify } = require('internal/util');

class Session extends inspector.Session {}
Session.prototype.post = promisify(inspector.Session.prototype.post);

module.exports = {
  ...inspector,
  Session,
};
