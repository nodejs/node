'use strict';

const { isSea, isVfsEnabled } = internalBinding('sea');
const { kEmptyObject } = require('internal/util');
const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

let initialized = false;

/* c8 ignore start -- SEA VFS initialization requires an actual SEA binary */

/**
 * Initializes the SEA virtual file system: a read-only VFS serving the
 * assets bundled into the executable, mounted at its reserved mount point.
 * Because a VFS never shadows the real file system, the assets live under
 * the mount point returned by `vfs.mountPoint`, not at a fixed path; the
 * SEA main script is placed at the mount point root so bundled code can
 * reach the assets through `__dirname`-relative paths and relative
 * `require()` calls.
 * @param {object} [options] Configuration options
 * @param {Record<string, string|Buffer>} [options.extraFiles] Additional
 *   files to serve alongside the assets (used for the SEA main script)
 * @returns {VirtualFileSystem|null} The mounted VFS, or null if not running
 *   as a SEA or VFS is not enabled in the SEA configuration
 * @throws {ERR_INVALID_STATE} If already initialized
 */
function initSeaVfs(options = kEmptyObject) {
  if (initialized) {
    throw new ERR_INVALID_STATE('SEA VFS is already initialized');
  }
  initialized = true;

  if (!isSea() || !isVfsEnabled()) {
    return null;
  }

  const { VirtualFileSystem } = require('internal/vfs/file_system');
  const { SEAProvider } = require('internal/vfs/providers/sea');

  const provider = new SEAProvider({ extraFiles: options.extraFiles });
  // The SEA warning already covers the feature; don't emit the
  // VirtualFileSystem experimental warning for the implicit SEA mount.
  const vfs = new VirtualFileSystem(provider, {
    emitExperimentalWarning: false,
  });
  vfs.mount();

  return vfs;
}

/* c8 ignore stop */

module.exports = {
  initSeaVfs,
};
