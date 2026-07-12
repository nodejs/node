'use strict';

module.exports = {
  async importFromInside(specifier) {
    return import(specifier);
  },
  async requireFromInside(specifier) {
    return { default: require(specifier) };
  },
};
