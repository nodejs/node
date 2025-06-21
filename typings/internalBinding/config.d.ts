export interface ConfigBinding {
  isDebugBuild: boolean;
  hasOpenSSL: boolean;
  fipsMode: boolean;
  hasIntl: boolean;
  hasTracing: boolean;
  hasNodeOptions: boolean;
  hasInspector: boolean;
  noBrowserGlobals: boolean;
  bits: number;
  hasDtrace: boolean;
}
