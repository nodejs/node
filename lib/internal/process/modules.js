'use strict';

const {
  setInitializeImportMetaObjectCallback
} = internalBinding('module_wrap');

function initializeImportMetaObject(wrap, meta) {
  meta.url = wrap.url;
}

function setupModules() {
  setInitializeImportMetaObjectCallback(initializeImportMetaObject);
}

module.exports = {
  setup: setupModules
};
