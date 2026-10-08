'use strict';

// Seam between node:ffi and the virtual file system, mirroring the fs
// handler integration in internal/fs/utils: the VFS hook installer sets a
// library reader while at least one VFS is mounted and clears it when the
// last one unmounts, and DynamicLibrary consults it before every load. The
// dependency points from the VFS into ffi: ffi never loads any VFS code,
// and pays only a null check while no VFS is mounted.

// When reader is null, no VFS is active (zero overhead). Otherwise it is
// (path) => Buffer|undefined: the library's bytes for a path inside a
// mounted VFS, or undefined for a path the dynamic loader should open
// itself.
let vfsLibraryReader = null;

function setVfsLibraryReader(reader) {
  vfsLibraryReader = reader;
}

function getVfsLibraryReader() {
  return vfsLibraryReader;
}

module.exports = {
  getVfsLibraryReader,
  setVfsLibraryReader,
};
