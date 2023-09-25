'use strict';

const pathModule = require('path');
const {
  getValidatedPath,
  stringToFlags,
  getValidMode,
  getStatsFromBinding,
  getStatFsFromBinding,
  getValidatedFd,
} = require('internal/fs/utils');
const {
  parseFileMode,
  validateObject,
  validateString,
} = require('internal/validators');
const { kEmptyObject } = require('internal/util');
const { createBlobFromFilePath } = require('internal/blob');

const binding = internalBinding('fs');

/**
 * @param {string} path
 * @param {number} flag
 * @return {string}
 */
function readFileUtf8(path, flag) {
  path = pathModule.toNamespacedPath(getValidatedPath(path));
  return binding.readFileUtf8(path, stringToFlags(flag));
}

function exists(path) {
  try {
    path = getValidatedPath(path);
  } catch {
    return false;
  }

  return binding.existsSync(pathModule.toNamespacedPath(path));
}

function access(path, mode) {
  path = getValidatedPath(path);
  mode = getValidMode(mode, 'access');

  binding.accessSync(pathModule.toNamespacedPath(path), mode);
}

function copyFile(src, dest, mode) {
  src = getValidatedPath(src, 'src');
  dest = getValidatedPath(dest, 'dest');

  binding.copyFileSync(
    pathModule.toNamespacedPath(src),
    pathModule.toNamespacedPath(dest),
    getValidMode(mode, 'copyFile'),
  );
}

function openAsBlob(path, options = kEmptyObject) {
  validateObject(options, 'options');
  const type = options.type || '';
  validateString(type, 'options.type');
  path = getValidatedPath(path);
  return createBlobFromFilePath(pathModule.toNamespacedPath(path), { type });
}

function stat(path, options = { bigint: false, throwIfNoEntry: true }) {
  path = getValidatedPath(path);
  const stats = binding.statSync(
    pathModule.toNamespacedPath(path),
    options.bigint,
    options.throwIfNoEntry,
  );
  if (stats === undefined) {
    return undefined;
  }
  return getStatsFromBinding(stats);
}

function statfs(path, options = { bigint: false }) {
  path = getValidatedPath(path);
  const stats = binding.statfsSync(pathModule.toNamespacedPath(path), options.bigint);
  return getStatFsFromBinding(stats);
}

function open(path, flags, mode) {
  path = getValidatedPath(path);

  return binding.openSync(
    pathModule.toNamespacedPath(path),
    stringToFlags(flags),
    parseFileMode(mode, 'mode', 0o666),
  );
}

function close(fd) {
  fd = getValidatedFd(fd);

  return binding.closeSync(fd);
}

module.exports = {
  readFileUtf8,
  exists,
  access,
  copyFile,
  openAsBlob,
  stat,
  statfs,
  open,
  close,
};
