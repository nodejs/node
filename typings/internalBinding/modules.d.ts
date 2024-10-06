export type PackageType = 'commonjs' | 'module' | 'none'
export type RecognizedPackageConfig = {
  name?: string
  main?: any
  type: PackageType
  exports?: string | string[] | Record<string, unknown>
  imports?: string | string[] | Record<string, unknown>
}
export type FullPackageConfig = RecognizedPackageConfig & {
  [key: string]: unknown,
}
export type DeserializedPackageConfig<everything = false> = {
  data: everything extends true ? FullPackageConfig : RecognizedPackageConfig
  path: URL['pathname'],
}
export type SerializedPackageConfig = [
  RecognizedPackageConfig['name'],
  RecognizedPackageConfig['main'],
  RecognizedPackageConfig['type'],
  string | undefined, // exports
  string | undefined, // imports
  DeserializedPackageConfig['path'], // pjson file path
]

export interface ModulesBinding {
  readPackageJSON(path: string): SerializedPackageConfig | undefined;
  getNearestParentPackageJSON(path: string): SerializedPackageConfig | undefined
  getNearestRawParentPackageJSON(origin: URL['pathname']): [ReturnType<JSON['stringify']>, DeserializedPackageConfig['path']] | undefined
  getNearestParentPackageJSONType(path: string): RecognizedPackageConfig['type']
  getPackageScopeConfig(path: string): SerializedPackageConfig | undefined
  getPackageJSONScripts(): string | undefined
}
