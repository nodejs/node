export type PackageType = 'commonjs' | 'module' | 'none'
export type PackageConfig = {
  pjsonPath: string
  exists: boolean
  name?: string
  main?: any
  type: PackageType
  exports?: string | string[] | Record<string, unknown>
  imports?: string | string[] | Record<string, unknown>
}
export type SerializedPackageConfig = [
  PackageConfig['name'],
  PackageConfig['main'],
  PackageConfig['type'],
  string | undefined, // exports
  string | undefined, // imports
  string | undefined, // raw json available for experimental policy
]

export interface ModulesBinding {
  readPackageJSON(path: string): SerializedPackageConfig | undefined;
  getNearestParentPackageJSON(path: string): PackageConfig | undefined
  getNearestParentPackageJSONType(path: string): [
    PackageConfig['type'],
    string, // package.json path
    string, // raw content
  ]
  getPackageScopeConfig(path: string): SerializedPackageConfig | undefined
}
