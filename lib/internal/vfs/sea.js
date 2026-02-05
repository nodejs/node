'use strict';

const { isSea, getAssetKeys } = internalBinding('sea');
const { kEmptyObject, getLazy } = require('internal/util');
const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

// Lazy-loaded VFS
let cachedSeaVfs = null;
let initialized = false;

// Lazy-load VirtualFileSystem and SEAProvider to avoid loading VFS code if not needed
const lazyVirtualFileSystem = getLazy(
  () => require('internal/vfs/file_system').VirtualFileSystem,
);
const lazySEAProvider = getLazy(
  () => require('internal/vfs/providers/sea').SEAProvider,
);

/**
 * Creates a VirtualFileSystem populated with SEA assets using the Provider architecture.
 * Assets are mounted at the specified prefix (default: '/sea').
 * This is an internal function - use initSeaVfs() or getSeaVfs() instead.
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
 * Initializes the SEA VFS with custom options.
 * Must be called before getSeaVfs() if custom options are needed.
 * @param {object} [options] Configuration options
 * @param {string} [options.prefix] Mount point prefix for SEA assets (default: '/sea')
 * @param {boolean} [options.moduleHooks] Whether to enable require/import hooks (default: true)
 * @returns {VirtualFileSystem|null} The VFS instance, or null if not running as SEA or no assets
 * @throws {ERR_INVALID_STATE} If already initialized
 */
function initSeaVfs(options = kEmptyObject) {
  if (initialized) {
    throw new ERR_INVALID_STATE('SEA VFS is already initialized');
  }

  initialized = true;
  cachedSeaVfs = createSeaVfs(options);
  return cachedSeaVfs;
}

/**
 * Gets the SEA VFS instance.
 * If not already initialized, auto-initializes with default options.
 * @returns {VirtualFileSystem|null} The VFS instance, or null if not running as SEA or no assets
 */
function getSeaVfs() {
  if (!initialized) {
    initialized = true;
    cachedSeaVfs = createSeaVfs();
  }
  return cachedSeaVfs;
}

module.exports = {
  createSeaVfs,
  initSeaVfs,
  getSeaVfs,
};
