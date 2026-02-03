'use strict';

const { isSea, getAssetKeys } = internalBinding('sea');
const { kEmptyObject, getLazy } = require('internal/util');

// Lazy-loaded VFS
let cachedSeaVfs = null;

// Lazy-load VirtualFileSystem and SEAProvider to avoid loading VFS code if not needed
const lazyVirtualFileSystem = getLazy(
  () => require('internal/vfs/file_system').VirtualFileSystem,
);
const lazySEAProvider = getLazy(
  () => require('internal/vfs/providers/sea').SEAProvider,
);

/**
 * Creates a VirtualFileSystem populated with SEA assets using the new Provider architecture.
 * Assets are mounted at the specified prefix (default: '/sea').
 * @param {object} [options] Configuration options
 * @param {string} [options.prefix] Mount point prefix for SEA assets
 * @param {boolean} [options.moduleHooks] Whether to enable require/import hooks
 * @returns {VirtualFileSystem|null} The VFS instance, or null if not running as SEA or no assets
 */
function createSeaVfs(options = kEmptyObject) {
  if (!isSea()) {
    return null;
  }

  // Only create VFS if there are VFS assets
  const keys = getAssetKeys();
  if (!keys || keys.length === 0) {
    return null;
  }

  const VirtualFileSystem = lazyVirtualFileSystem();
  const SEAProvider = lazySEAProvider();

  const prefix = options.prefix ?? '/sea';
  const moduleHooks = options.moduleHooks !== false;

  const provider = new SEAProvider();
  const vfs = new VirtualFileSystem(provider, { moduleHooks });

  // Mount at the specified prefix
  vfs.mount(prefix);

  return vfs;
}

/**
 * Gets or creates the default SEA VFS instance.
 * This is a singleton that is lazily created on first access.
 * @param {object} [options] Configuration options (only used on first call)
 * @returns {VirtualFileSystem|null} The VFS instance, or null if not running as SEA
 */
function getSeaVfs(options) {
  if (cachedSeaVfs === null) {
    cachedSeaVfs = createSeaVfs(options);
  }
  return cachedSeaVfs;
}

module.exports = {
  createSeaVfs,
  getSeaVfs,
};
