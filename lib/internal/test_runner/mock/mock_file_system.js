'use strict';

const {
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  SafeMap,
  SymbolDispose,
} = primordials;

const { validateObject, validateBoolean } = require('internal/validators');

const { ERR_INVALID_STATE, ERR_NO_SUCH_FILE } = require('internal/errors');

const fs = require('fs');

const { Buffer } = require('buffer');

// Mock file system implementation
class MockFileSystem {
  #realFsReadFileSync;
  #realFsWriteFileSync;
  #realFsExistsSync;
  #realFsUnlinkSync;
  #realFsStatSync;

  #files = new SafeMap();
  #isEnabled = false;
  #override = false;

  constructor() {
    this.#storeOriginalFsMethods();
  }

  #storeOriginalFsMethods() {
    this.#realFsReadFileSync = ObjectGetOwnPropertyDescriptor(
      fs,
      'readFileSync'
    );
    this.#realFsWriteFileSync = ObjectGetOwnPropertyDescriptor(
      fs,
      'writeFileSync'
    );
    this.#realFsExistsSync = ObjectGetOwnPropertyDescriptor(fs, 'existsSync');
    this.#realFsUnlinkSync = ObjectGetOwnPropertyDescriptor(fs, 'unlinkSync');
    this.#realFsStatSync = ObjectGetOwnPropertyDescriptor(fs, 'statSync');
  }

  #restoreOriginalFsMethods() {
    ObjectDefineProperty(fs, 'readFileSync', this.#realFsReadFileSync);
    ObjectDefineProperty(fs, 'writeFileSync', this.#realFsWriteFileSync);
    ObjectDefineProperty(fs, 'existsSync', this.#realFsExistsSync);
    ObjectDefineProperty(fs, 'unlinkSync', this.#realFsUnlinkSync);
    ObjectDefineProperty(fs, 'statSync', this.#realFsStatSync);
  }

  #mockReadFileSync(path, options) {
    if (this.#files.has(path)) {
      const content = this.#files.get(path);
      return content instanceof Buffer ? content : Buffer.from(content);
    }
    if (this.#override) {
      throw new ERR_NO_SUCH_FILE(
        `ENOENT: no such file or directory, open '${path}'`
      );
    }
    return this.#realFsReadFileSync.value(path, options);
  }

  #mockWriteFileSync(path, data, options) {
    this.#files.set(path, Buffer.from(data));
  }

  #mockExistsSync(path) {
    if (this.#files.has(path)) {
      return true;
    }
    if (this.#override) {
      return false;
    }
    return this.#realFsExistsSync.value(path);
  }

  #mockUnlinkSync(path) {
    this.#assertFileSystemIsEnabled();

    if (this.#files.has(path)) {
      this.#files.delete(path);
    } else if (this.#override) {
      throw new ERR_NO_SUCH_FILE(
        `ENOENT: no such file or directory, unlink '${path}'`
      );
    } else {
      this.#realFsUnlinkSync.value(path);
    }
  }

  #mockStatSync(path) {
    this.#assertFileSystemIsEnabled();

    if (this.#files.has(path)) {
      return {
        isFile: () => true,
        isDirectory: () => false,
      };
    } else if (this.#override) {
      throw new ERR_NO_SUCH_FILE(
        `ENOENT: no such file or directory, stat '${path}'`
      );
    }
    return this.#realFsStatSync.value(path);
  }

  #toggleEnableFileSystem(activate, config) {
    if (activate) {
      if (config?.files) {
        for (const [path, content] of Object.entries(config.files)) {
          this.#files.set(
            path,
            content instanceof Buffer ? content : Buffer.from(content)
          );
        }
      }
      this.#override = config?.override || false;

      ObjectDefineProperty(fs, 'readFileSync', {
        value: (path, options) => this.#mockReadFileSync(path, options),
        writable: true,
        configurable: true,
      });
      ObjectDefineProperty(fs, 'writeFileSync', {
        value: (path, data, options) =>
          this.#mockWriteFileSync(path, data, options),
        writable: true,
        configurable: true,
      });
      ObjectDefineProperty(fs, 'existsSync', {
        value: (path) => this.#mockExistsSync(path),
        writable: true,
        configurable: true,
      });
    } else {
      this.#restoreOriginalFsMethods();
      this.#files.clear();
      this.#override = false;
    }
    this.#isEnabled = activate;
  }

  #assertFileSystemIsEnabled() {
    if (!this.#isEnabled) {
      throw new ERR_INVALID_STATE(
        'MockFileSystem is not enabled. Call .enable() first.'
      );
    }
  }

  /**
   * Enables the mock file system, replacing native fs methods with mocked ones.
   * @param {Object} [options]
   * @param {Object.<string, Buffer|Uint8Array|string>} [options.files] - Virtual files to initialize.
   * @param {boolean} [options.override] - If true, only mock files are accessible.
   */
  enable(options = { __proto__: null, files: {}, override: false }) {
    if (this.#isEnabled) {
      throw new ERR_INVALID_STATE('MockFileSystem is already enabled.');
    }
    validateObject(options, 'options');
    validateObject(options.files, 'options.files');
    validateBoolean(options.override, 'options.override');
    this.#toggleEnableFileSystem(true, options);
  }

  /**
   * Disables the mock file system, restoring native fs methods.
   */
  disable() {
    if (!this.#isEnabled) return;
    this.#toggleEnableFileSystem(false);
  }

  /**
   * Disposes the mock file system, alias for disable.
   */
  [SymbolDispose]() {
    this.disable();
  }

  /**
   * Reads the content of a file from the mock file system.
   * @param {string} path - The file path to read.
   * @param {Object|string} [options] - Encoding or options for reading.
   * @returns {Buffer|string} The file content.
   */
  readFileSync(path, options) {
    this.#assertFileSystemIsEnabled();
    return this.#mockReadFileSync(path, options);
  }

  /**
   * Writes content to a file in the mock file system.
   * @param {string} path - The file path to write.
   * @param {Buffer|string} data - The data to write.
   */
  writeFileSync(path, data) {
    this.#assertFileSystemIsEnabled();
    this.#mockWriteFileSync(path, data);
  }

  /**
   * Checks if a file exists in the mock file system.
   * @param {string} path - The file path to check.
   * @returns {boolean} True if the file exists, false otherwise.
   */
  existsSync(path) {
    this.#assertFileSystemIsEnabled();
    return this.#mockExistsSync(path);
  }

  /**
   * Deletes a file from the mock file system.
   * @param {string} path - The file path to delete.
   */
  unlinkSync(path) {
    this.#assertFileSystemIsEnabled();
    this.#mockUnlinkSync(path);
  }

  /**
   * Gets the stats of a file in the mock file system.
   * @param {string} path - The file path to get stats for.
   * @returns {Object} An object with isFile and isDirectory methods.
   */
  statSync(path) {
    this.#assertFileSystemIsEnabled();
    return this.#mockStatSync(path);
  }
}

module.exports = { MockFileSystem };
