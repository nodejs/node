'use strict';

const inspector = require('inspector');

const session = new inspector.Session();
session.connect();

const makeProxy = (name) => new Proxy({}, {
  get: (target, method) => {
    const n = `${name}.${method}`;
    return (params = {}) => {
      let r;
      session.post(n, params, (err, result) => {
        if (err) {
          // eslint-disable-next-line no-restricted-syntax
          throw new Error(err.message);
        }
        r = result;
      });
      return r;
    };
  },
});

module.exports = {
  Runtime: makeProxy('Runtime'),
};
