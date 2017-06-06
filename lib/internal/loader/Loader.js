'use strict';

const { URL } = require('url');
const { getURLFromFilePath } = require('internal/url');

const {
  getNamespaceOfModuleWrap
} = require('internal/loader/ModuleWrap');

const ModuleMap = require('internal/loader/ModuleMap');
const ModuleJob = require('internal/loader/ModuleJob');
const resolveRequestUrl = require('internal/loader/resolveRequestUrl');
const errors = require('internal/errors');

function getBase() {
  try {
    return getURLFromFilePath(`${process.cwd()}/`);
  } catch (e) {
    e.stack;
    // If the current working directory no longer exists.
    if (e.code === 'ENOENT') {
      return undefined;
    }
    throw e;
  }
}

class Loader {
  constructor(base = getBase()) {
    this.moduleMap = new ModuleMap();
    if (typeof base !== 'undefined' && base instanceof URL !== true) {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'base', 'URL');
    }
    this.base = base;
  }

  async resolve(specifier) {
    const request = resolveRequestUrl(this.base, specifier);
    if (request.url.protocol !== 'file:') {
      throw new errors.Error('ERR_INVALID_PROTOCOL',
                             request.url.protocol, 'file:');
    }
    return request.url;
  }

  async getModuleJob(dependentJob, specifier) {
    if (!this.moduleMap.has(dependentJob.url)) {
      throw new errors.Error('ERR_MISSING_MODULE', dependentJob.url);
    }
    const request = await resolveRequestUrl(dependentJob.url, specifier);
    const url = `${request.url}`;
    if (this.moduleMap.has(url)) {
      return this.moduleMap.get(url);
    }
    const dependencyJob = new ModuleJob(this, request);
    this.moduleMap.set(url, dependencyJob);
    return dependencyJob;
  }

  async import(specifier) {
    const request = await resolveRequestUrl(this.base, specifier);
    const url = `${request.url}`;
    let job;
    if (this.moduleMap.has(url)) {
      job = this.moduleMap.get(url);
    } else {
      job = new ModuleJob(this, request);
      this.moduleMap.set(url, job);
    }
    const module = await job.run();
    return getNamespaceOfModuleWrap(module);
  }
}
Object.setPrototypeOf(Loader.prototype, null);
module.exports = Loader;
