export type PackageType = 'commonjs' | 'module' | 'none'
export type RecognisedPackageConfig = {
  pjsonPath: string
  exists: boolean
  name?: string
  main?: any
  type: PackageType
  exports?: string | string[] | Record<string, unknown>
  imports?: string | string[] | Record<string, unknown>
}
export type FullPackageConfig = RecognisedPackageConfig & {
  [key: string]: unknown,
}
export type SerializedPackageConfig = [
  RecognisedPackageConfig['name'],
  RecognisedPackageConfig['main'],
  RecognisedPackageConfig['type'],
  string | undefined, // exports
  string | undefined, // imports
  string | undefined, // raw json available for experimental policy
]

export interface ModulesBinding {
  readPackageJSON(path: string): SerializedPackageConfig | undefined;
  getNearestParentPackageJSON(path: string): RecognisedPackageConfig | undefined
  getNearestRawParentPackageJSON(origin: URL['pathname']): [ReturnType<JSON['stringify']>, RecognisedPackageConfig['pjsonPath']] | undefined
  getNearestParentPackageJSONType(path: string): RecognisedPackageConfig['type']
  getPackageScopeConfig(path: string): SerializedPackageConfig | undefined
  getPackageJSONScripts(): string | undefined
}
