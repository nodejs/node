export type PackageType = 'commonjs' | 'module' | 'none'
export type PackageConfig = {
  name?: string
  main?: any
  type: PackageType
  exports?: string | string[] | Record<string, unknown>
  imports?: string | string[] | Record<string, unknown>
}
export type DeserializedPackageConfig = {
  data: PackageConfig,
  exists: boolean,
  path: URL['pathname'],
}
export type SerializedPackageConfig = [
  PackageConfig['name'],
  PackageConfig['main'],
  PackageConfig['type'],
  string | undefined, // exports
  string | undefined, // imports
  DeserializedPackageConfig['path'], // pjson file path
]

export interface ModulesBinding {
  readPackageJSON(path: string): SerializedPackageConfig | undefined;
  getNearestParentPackageJSON(path: string): SerializedPackageConfig | undefined
  getNearestRawParentPackageJSON(origin: URL['pathname']): [ReturnType<JSON['stringify']>, DeserializedPackageConfig['path']] | undefined
  getNearestParentPackageJSONType(path: string): PackageConfig['type']
  getPackageScopeConfig(path: string): SerializedPackageConfig | undefined
  getPackageType(path: string): PackageConfig['type'] | undefined
  enableCompileCache(path?: string): { status: number, message?: string, directory?: string }
  getCompileCacheDir(): string | undefined
  flushCompileCache(keepDeserializedCache?: boolean): void
}
