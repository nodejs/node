'use strict';

const {
  SafeMap,
  Symbol,
} = primordials;

// Private symbols
const kFd = Symbol('kFd');
const kEntry = Symbol('kEntry');
const kPosition = Symbol('kPosition');
const kFlags = Symbol('kFlags');
const kContent = Symbol('kContent');
const kPath = Symbol('kPath');

// FD range: 10000+ to avoid conflicts with real fds
const VFS_FD_BASE = 10_000;
let nextFd = VFS_FD_BASE;

// Global registry of open virtual file descriptors
const openFDs = new SafeMap();

/**
 * Represents an open virtual file descriptor.
 */
class VirtualFD {
  /**
   * @param {number} fd The file descriptor number
   * @param {VirtualFile} entry The virtual file entry
   * @param {string} flags The open flags (r, r+, w, w+, a, a+)
   * @param {string} path The path used to open the file
   */
  constructor(fd, entry, flags, path) {
    this[kFd] = fd;
    this[kEntry] = entry;
    this[kPosition] = 0;
    this[kFlags] = flags;
    this[kPath] = path;
    this[kContent] = null; // Cached content buffer
  }

  /**
   * Gets the file descriptor number.
   * @returns {number}
   */
  get fd() {
    return this[kFd];
  }

  /**
   * Gets the file entry.
   * @returns {VirtualFile}
   */
  get entry() {
    return this[kEntry];
  }

  /**
   * Gets the current position.
   * @returns {number}
   */
  get position() {
    return this[kPosition];
  }

  /**
   * Sets the current position.
   * @param {number} pos The new position
   */
  set position(pos) {
    this[kPosition] = pos;
  }

  /**
   * Gets the open flags.
   * @returns {string}
   */
  get flags() {
    return this[kFlags];
  }

  /**
   * Gets the path used to open the file.
   * @returns {string}
   */
  get path() {
    return this[kPath];
  }

  /**
   * Gets or loads the cached content buffer.
   * @returns {Buffer}
   */
  getContentSync() {
    this[kContent] ??= this[kEntry].getContentSync();
    return this[kContent];
  }

  /**
   * Gets or loads the cached content buffer asynchronously.
   * @returns {Promise<Buffer>}
   */
  async getContent() {
    this[kContent] ??= await this[kEntry].getContent();
    return this[kContent];
  }
}

/**
 * Opens a virtual file and returns its file descriptor.
 * @param {VirtualFile} entry The virtual file entry
 * @param {string} flags The open flags
 * @param {string} path The path used to open the file
 * @returns {number} The file descriptor
 */
function openVirtualFd(entry, flags, path) {
  const fd = nextFd++;
  const vfd = new VirtualFD(fd, entry, flags, path);
  openFDs.set(fd, vfd);
  return fd;
}

/**
 * Gets a VirtualFD by its file descriptor number.
 * @param {number} fd The file descriptor number
 * @returns {VirtualFD|undefined}
 */
function getVirtualFd(fd) {
  return openFDs.get(fd);
}

/**
 * Closes a virtual file descriptor.
 * @param {number} fd The file descriptor number
 * @returns {boolean} True if the fd was found and closed
 */
function closeVirtualFd(fd) {
  return openFDs.delete(fd);
}

/**
 * Checks if a file descriptor is a virtual fd.
 * @param {number} fd The file descriptor number
 * @returns {boolean}
 */
function isVirtualFd(fd) {
  return openFDs.has(fd);
}

/**
 * Gets the count of open virtual file descriptors.
 * @returns {number}
 */
function getOpenFdCount() {
  return openFDs.size;
}

module.exports = {
  VirtualFD,
  VFS_FD_BASE,
  openVirtualFd,
  getVirtualFd,
  closeVirtualFd,
  isVirtualFd,
  getOpenFdCount,
};
