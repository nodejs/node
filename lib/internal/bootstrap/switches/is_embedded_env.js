'use strict';

// This is the bootstrapping code used when creating a new environment
// through N-API

// Set up globalThis.require and globalThis.import so that they can
// be easily accessed from C/C++

/* global path, primordials */

const { globalThis, ObjectCreate } = primordials;
const CJSLoader = require('internal/modules/cjs/loader');
const ESMLoader = require('internal/modules/esm/loader').ESMLoader;

globalThis.module = new CJSLoader.Module();
globalThis.require = require('module').createRequire(path);

const internalLoader = new ESMLoader();
const parent_path = require('url').pathToFileURL(path);
globalThis.import = (mod) => internalLoader.import(mod, parent_path, ObjectCreate(null));
globalThis.import.meta = { url: parent_path };
