'use strict';

const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { VirtualFileSystem } = require('internal/vfs/file_system');
const { VirtualProvider } = require('internal/vfs/provider');
const { MemoryProvider } = require('internal/vfs/providers/memory');

// SEAProvider is lazy-loaded to avoid loading SEA bindings when not needed
let SEAProvider = null;

function getSEAProvider() {
  if (SEAProvider === null) {
    try {
      SEAProvider = require('internal/vfs/providers/sea').SEAProvider;
    } catch {
      // SEA bindings not available (not running in SEA)
      SEAProvider = class SEAProviderUnavailable {
        constructor() {
          throw new ERR_INVALID_STATE('SEAProvider can only be used in a Single Executable Application');
        }
      };
    }
  }
  return SEAProvider;
}

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

/**
 * Creates a VirtualFileSystem with SEA assets mounted.
 * Only works when running as a Single Executable Application.
 * @param {object} [options] Configuration options
 * @param {string} [options.mountPoint] Mount point path (default: '/sea')
 * @param {boolean} [options.moduleHooks] Whether to enable require/import hooks (default: true)
 * @param {boolean} [options.virtualCwd] Whether to enable virtual working directory
 * @returns {VirtualFileSystem|null} The VFS instance, or null if not running as SEA
 */
function createSEA(options = {}) {
  const SEAProviderClass = getSEAProvider();
  try {
    const provider = new SEAProviderClass();
    const vfs = new VirtualFileSystem(provider, {
      moduleHooks: options.moduleHooks,
      virtualCwd: options.virtualCwd,
    });
    vfs.mount(options.mountPoint ?? '/sea');
    return vfs;
  } catch {
    return null;
  }
}

module.exports = {
  create,
  createSEA,
  VirtualFileSystem,
  VirtualProvider,
  MemoryProvider,
  get SEAProvider() {
    return getSEAProvider();
  },
};
