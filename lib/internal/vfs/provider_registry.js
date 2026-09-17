'use strict';

// Maps a `--vfs-mount` source to the provider that backs it. Directories are
// served by RealFSProvider and ZIP archives by ZipProvider; both are claimed
// from the source itself (a stat, or a trial open) rather than its file
// extension. Any other source type is added via node:vfs's registerProvider().

const {
  ArrayPrototypeUnshift,
} = primordials;
const {
  validateFunction,
  validateObject,
  validateString,
} = require('internal/validators');

// A source is claimed by the first provider whose canHandle() returns true.
// Registered providers are unshifted ahead of these built-ins so a custom
// provider can back, wrap, or vet any source. Requires are deferred to the
// provider methods so the zlib/zip and fs-provider machinery stays off the
// startup path until a mount actually needs it.
//
// ZipFile.openSync() locates the end-of-central-directory record and throws
// when the source is not a ZIP, so it doubles as the content check; the opened
// archive is stashed and handed to the provider rather than reopened.
let pendingZipFile = null;
const providers = [
  {
    name: 'zip',
    canHandle(resolvedPath, stats) {
      if (!stats.isFile()) return false;
      const { ZipFile } = require('internal/zip');
      try {
        pendingZipFile = ZipFile.openSync(resolvedPath);
        return true;
      } catch {
        pendingZipFile = null;
        return false;
      }
    },
    create(resolvedPath) {
      const { ZipFile } = require('internal/zip');
      const { ZipProvider } = require('internal/vfs/providers/ziparchive');
      const source = pendingZipFile ?? ZipFile.openSync(resolvedPath);
      pendingZipFile = null;
      return new ZipProvider(source);
    },
  },
  {
    name: 'dir',
    canHandle(resolvedPath, stats) { return stats.isDirectory(); },
    create(resolvedPath) {
      const { RealFSProvider } = require('internal/vfs/providers/real');
      return new RealFSProvider(resolvedPath);
    },
  },
];

/**
 * Registers a provider that `--vfs-mount` can select for a source it
 * recognizes. The newest registration is consulted first, and all registered
 * providers outrank the built-in directory provider, so a custom provider can
 * back, wrap, or vet any mount.
 * @param {object} entry
 * @param {string} entry.name A short identifier, used in diagnostics.
 * @param {(resolvedPath: string, stats: object) => boolean} entry.canHandle
 *   Returns `true` if this provider should back `resolvedPath`.
 * @param {(resolvedPath: string, stats: object) => object} entry.create
 *   Returns the VirtualProvider backing `resolvedPath`.
 */
function registerProvider(entry) {
  validateObject(entry, 'entry');
  validateString(entry.name, 'entry.name');
  validateFunction(entry.canHandle, 'entry.canHandle');
  validateFunction(entry.create, 'entry.create');
  ArrayPrototypeUnshift(providers, {
    name: entry.name,
    canHandle: entry.canHandle,
    create: entry.create,
  });
}

function selectProvider(resolvedPath, stats) {
  for (let i = 0; i < providers.length; i++) {
    if (providers[i].canHandle(resolvedPath, stats)) {
      return providers[i].create(resolvedPath, stats);
    }
  }
  return null;
}

module.exports = {
  registerProvider,
  selectProvider,
};
