export namespace codes {
  const ERR_INVALID_MODULE_SPECIFIER: new (...args: unknown[]) => Error
  const ERR_INVALID_PACKAGE_CONFIG: new (...args: unknown[]) => Error
  const ERR_INVALID_PACKAGE_TARGET: new (...args: unknown[]) => Error
  const ERR_MODULE_NOT_FOUND: new (...args: unknown[]) => Error
  const ERR_PACKAGE_IMPORT_NOT_DEFINED: new (...args: unknown[]) => Error
  const ERR_PACKAGE_PATH_NOT_EXPORTED: new (...args: unknown[]) => Error
  const ERR_UNSUPPORTED_DIR_IMPORT: new (...args: unknown[]) => Error
  const ERR_UNKNOWN_FILE_EXTENSION: new (...args: unknown[]) => Error
  const ERR_INVALID_ARG_VALUE: new (...args: unknown[]) => Error
  const ERR_UNSUPPORTED_ESM_URL_SCHEME: new (...args: unknown[]) => Error
}
export type MessageFunction = (...args: unknown[]) => string
