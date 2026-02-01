'use strict';

const {
  SafeMap,
  Symbol,
} = primordials;

// Private symbols
const kFd = Symbol('kFd');
const kEntry = Symbol('kEntry');

// FD range: 10000+ to avoid conflicts with real fds
const VFS_FD_BASE = 10_000;
let nextFd = VFS_FD_BASE;

// Global registry of open virtual file descriptors
const openFDs = new SafeMap();

/**
 * Represents an open virtual file descriptor.
 * Wraps a VirtualFileHandle from the provider.
 */
class VirtualFD {
  /**
   * @param {number} fd The file descriptor number
   * @param {VirtualFileHandle} entry The virtual file handle
   */
  constructor(fd, entry) {
    this[kFd] = fd;
    this[kEntry] = entry;
  }

  /**
   * Gets the file descriptor number.
   * @returns {number}
   */
  get fd() {
    return this[kFd];
  }

  /**
   * Gets the file handle.
   * @returns {VirtualFileHandle}
   */
  get entry() {
    return this[kEntry];
  }
}

/**
 * Opens a virtual file and returns its file descriptor.
 * @param {VirtualFileHandle} entry The virtual file handle
 * @returns {number} The file descriptor
 */
function openVirtualFd(entry) {
  const fd = nextFd++;
  const vfd = new VirtualFD(fd, entry);
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

module.exports = {
  VirtualFD,
  VFS_FD_BASE,
  openVirtualFd,
  getVirtualFd,
  closeVirtualFd,
};
