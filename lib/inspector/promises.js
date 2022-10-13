'use strict';

const inspector = require('inspector');
const { promisify } = require('util');
const { FunctionPrototypeBind } = primordials;
class Session extends inspector.Session {
  #post = promisify(FunctionPrototypeBind(super.post, this));
  /**
   * Posts a message to the inspector back-end.
   * @param {string} method
   * @param {Record<unknown, unknown>} [params]
   * @returns {Promise}
   */
  async post(method, params) {
    return this.#post(method, params);
  }
}

module.exports = {
  ...inspector,
  Session,
};
