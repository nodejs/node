'use strict';

// This is the bootstrapping code used when creating a new environment
// through N-API

// Set up globalThis.require and globalThis.import so that they can
// be easily accessed from C/C++

/* global path, primordials */

const { globalThis } = primordials;
const cjsLoader = require('internal/modules/cjs/loader');
const esmLoader = require('internal/process/esm_loader').esmLoader;

globalThis.module = new cjsLoader.Module();
globalThis.require = require('module').createRequire(path);

const parent_path = require('url').pathToFileURL(path);
globalThis.import = (mod) => esmLoader.import(mod, parent_path, { __proto__: null });
globalThis.import.meta = { url: parent_path };
