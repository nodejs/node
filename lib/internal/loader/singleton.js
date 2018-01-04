'use strict';

const Loader = require('internal/loader/Loader');

function newLoader(hooks) {
  const loader = new Loader();
  Loader.registerImportDynamicallyCallback(loader);
  if (hooks)
    loader.hook(hooks);

  return loader;
}

module.exports = {
  loaded: false,
  get loader() {
    delete this.loader;
    this.loader = newLoader();
    this.loaded = true;
    return this.loader;
  },
  replace(hooks) {
    this.loader = newLoader(hooks);
  },
};
