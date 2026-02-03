'use strict';

const { VirtualFileSystem } = require('internal/vfs/file_system');
const { VirtualProvider } = require('internal/vfs/provider');
const { MemoryProvider } = require('internal/vfs/providers/memory');
const { RealFSProvider } = require('internal/vfs/providers/real');

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
  if (provider !== undefined && provider !== null &&
      !(provider instanceof VirtualProvider) &&
      typeof provider === 'object') {
    options = provider;
    provider = undefined;
  }
  return new VirtualFileSystem(provider, options);
}

module.exports = {
  create,
  VirtualFileSystem,
  VirtualProvider,
  MemoryProvider,
  RealFSProvider,
};
