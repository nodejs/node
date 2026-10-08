'use strict';

const {
  ObjectKeys,
} = primordials;

const {
  isSea,
  isVfsEnabled,
  isVfsArchiveEnabled,
  getAsset,
  getMainCode,
  mainCodePath,
} = internalBinding('sea');
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
 * @param {string|Buffer} mainCode The source of the SEA main script
 * @returns {VirtualFileSystem|null} The mounted VFS, or null if not running
 *   as a SEA or VFS is not enabled in the SEA configuration
 * @throws {ERR_INVALID_STATE} If already initialized
 */
function initSeaVfs(mainCode) {
  if (initialized) {
    throw new ERR_INVALID_STATE('SEA VFS is already initialized');
  }
  initialized = true;

  if (!isSea() || !isVfsEnabled()) {
    return null;
  }

  const { VirtualFileSystem, kLoadLayer } = require('internal/vfs/file_system');

  const extraFiles = { __proto__: null, [getSeaMainName()]: mainCode };
  const isArchive = isVfsArchiveEnabled();
  let provider;
  if (isArchive) {
    provider = createZipProvider(extraFiles);
  } else {
    const { SEAProvider } = require('internal/vfs/providers/sea');
    provider = new SEAProvider({ extraFiles });
  }
  // The SEA warning already covers the feature; don't emit the
  // VirtualFileSystem experimental warning for the implicit SEA mount.
  // An archive is mounted where --vfs-load mounts its source, which is the
  // same mount point in every thread, so a worker can reach the code in it.
  const vfs = new VirtualFileSystem(provider, {
    __proto__: null,
    emitExperimentalWarning: false,
    [kLoadLayer]: isArchive,
  });
  vfs.mount();

  return vfs;
}

/**
 * Mounts the SEA virtual file system in a worker, when it is backed by a
 * `"vfsArchive"`, so that a path into the mount in the main thread, such as
 * the main script's `__filename`, is also valid in the worker.
 */
function initSeaVfsInWorker() {
  if (isVfsArchiveEnabled()) {
    initSeaVfs(getMainCode());
  }
}

/**
 * The name under which the SEA main script is placed at the mount point root.
 * @returns {string}
 */
function getSeaMainName() {
  const { basename } = require('path');
  return basename(mainCodePath || process.execPath);
}

// The reserved asset key under which --build-sea stores the ZIP archive
// named by "vfsArchive". Must match kVfsArchiveAssetName in src/node_sea.cc.
const kVfsArchiveAssetName = 'node:sea:vfs.zip';

/**
 * Creates a ZipProvider over the ZIP archive embedded by `"vfsArchive"`.
 * The archive bytes are used in place (a zero-copy view over the SEA
 * blob); entries are inflated on demand when they are opened.
 * The extra files (the SEA main script) are added to the in-memory archive
 * index as stored entries, leaving the embedded bytes untouched.
 * @param {Record<string, string|Buffer>} [extraFiles] Additional files to
 *   serve alongside the assets (used for the SEA main script)
 * @returns {ZipProvider}
 */
function createZipProvider(extraFiles) {
  const { Buffer } = require('buffer');
  const { ZipBuffer } = require('internal/zip');
  const { ZipProvider } = require('internal/vfs/providers/ziparchive');

  // getAsset returns a zero-copy ArrayBuffer over the (possibly read-only)
  // SEA blob; ZipBuffer only reads from it, and decompressed contents are
  // fresh buffers, so the view can be used without copying the archive.
  const archive = getAsset(kVfsArchiveAssetName);
  const zip = new ZipBuffer(Buffer.from(archive));

  if (extraFiles !== undefined) {
    const names = ObjectKeys(extraFiles);
    for (let i = 0; i < names.length; i++) {
      const content = extraFiles[names[i]];
      const data = typeof content === 'string' ? Buffer.from(content) : content;
      zip.addSync(names[i], data, { __proto__: null, method: 'store' });
    }
  }

  return new ZipProvider(zip);
}

/* c8 ignore stop */

module.exports = {
  getSeaMainName,
  initSeaVfs,
  initSeaVfsInWorker,
};
