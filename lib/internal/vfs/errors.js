'use strict';

const {
  ErrorCaptureStackTrace,
} = primordials;

const {
  UVException,
} = require('internal/errors');

const {
  UV_ENOENT,
  UV_ENOTDIR,
  UV_ENOTEMPTY,
  UV_EISDIR,
  UV_EBADF,
  UV_EEXIST,
  UV_EROFS,
  UV_EINVAL,
  UV_ELOOP,
} = internalBinding('uv');

/**
 * Creates an ENOENT error for virtual file system operations.
 * @param {string} syscall The system call name
 * @param {string} path The path that was not found
 * @returns {Error}
 */
function createENOENT(syscall, path) {
  const err = new UVException({
    errno: UV_ENOENT,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createENOENT);
  return err;
}

/**
 * Creates an ENOTDIR error.
 * @param {string} syscall The system call name
 * @param {string} path The path that is not a directory
 * @returns {Error}
 */
function createENOTDIR(syscall, path) {
  const err = new UVException({
    errno: UV_ENOTDIR,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createENOTDIR);
  return err;
}

/**
 * Creates an ENOTEMPTY error for non-empty directory.
 * @param {string} syscall The system call name
 * @param {string} path The path of the non-empty directory
 * @returns {Error}
 */
function createENOTEMPTY(syscall, path) {
  const err = new UVException({
    errno: UV_ENOTEMPTY,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createENOTEMPTY);
  return err;
}

/**
 * Creates an EISDIR error.
 * @param {string} syscall The system call name
 * @param {string} path The path that is a directory
 * @returns {Error}
 */
function createEISDIR(syscall, path) {
  const err = new UVException({
    errno: UV_EISDIR,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createEISDIR);
  return err;
}

/**
 * Creates an EBADF error for invalid file descriptor operations.
 * @param {string} syscall The system call name
 * @returns {Error}
 */
function createEBADF(syscall) {
  const err = new UVException({
    errno: UV_EBADF,
    syscall,
  });
  ErrorCaptureStackTrace(err, createEBADF);
  return err;
}

/**
 * Creates an EEXIST error.
 * @param {string} syscall The system call name
 * @param {string} path The path that already exists
 * @returns {Error}
 */
function createEEXIST(syscall, path) {
  const err = new UVException({
    errno: UV_EEXIST,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createEEXIST);
  return err;
}

/**
 * Creates an EROFS error for read-only file system.
 * @param {string} syscall The system call name
 * @param {string} path The path
 * @returns {Error}
 */
function createEROFS(syscall, path) {
  const err = new UVException({
    errno: UV_EROFS,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createEROFS);
  return err;
}

/**
 * Creates an EINVAL error for invalid argument.
 * @param {string} syscall The system call name
 * @param {string} path The path
 * @returns {Error}
 */
function createEINVAL(syscall, path) {
  const err = new UVException({
    errno: UV_EINVAL,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createEINVAL);
  return err;
}

/**
 * Creates an ELOOP error for too many symbolic links.
 * @param {string} syscall The system call name
 * @param {string} path The path
 * @returns {Error}
 */
function createELOOP(syscall, path) {
  const err = new UVException({
    errno: UV_ELOOP,
    syscall,
    path,
  });
  ErrorCaptureStackTrace(err, createELOOP);
  return err;
}

module.exports = {
  createENOENT,
  createENOTDIR,
  createENOTEMPTY,
  createEISDIR,
  createEBADF,
  createEEXIST,
  createEROFS,
  createEINVAL,
  createELOOP,
};
