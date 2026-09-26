'use strict';

const {
  FunctionPrototypeSymbolHasInstance,
} = primordials;

const { VirtualFileSystem } = require('internal/vfs/file_system');
const { VirtualProvider } = require('internal/vfs/provider');
const { MemoryProvider } = require('internal/vfs/providers/memory');
const { ComposableProvider } = require('internal/vfs/providers/composable');
const { RealFSProvider } = require('internal/vfs/providers/real');
const { ZipProvider } = require('internal/vfs/providers/ziparchive');
const { registerProvider } = require('internal/vfs/provider_registry');
const { getVfsRoot } = require('internal/vfs/router');

/**
 * Creates a new VirtualFileSystem instance.
 * @param {VirtualProvider} [provider] The provider to use (defaults to MemoryProvider)
 * @param {object} [options] Configuration options
 * @param {boolean} [options.moduleHooks] Whether to enable require/import hooks (default: true)
 * @param {boolean} [options.virtualCwd] Whether to enable virtual working directory
 * @returns {VirtualFileSystem}
 */
function create(provider, options) {
  // Handle case where first arg is options (no provider)
  if (provider != null &&
      !FunctionPrototypeSymbolHasInstance(VirtualProvider, provider) &&
      typeof provider === 'object') {
    options = provider;
    provider = undefined;
  }
  return new VirtualFileSystem(provider, options);
}

/**
 * Returns the directory that holds the mount points of every mounted virtual
 * file system.
 * @returns {string} The absolute path of the reserved root directory
 */
function vfsBase() {
  return getVfsRoot();
}

module.exports = {
  create,
  vfsBase,
  registerProvider,
  VirtualFileSystem,
  VirtualProvider,
  MemoryProvider,
  ComposableProvider,
  RealFSProvider,
  ZipProvider,
};
