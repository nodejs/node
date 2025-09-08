export interface ConfigBinding {
  isDebugBuild: boolean;
  openSSLIsBoringSSL: boolean;
  hasOpenSSL: boolean;
  fipsMode: boolean;
  hasIntl: boolean;
  hasSmallICU: boolean;
  hasTracing: boolean;
  hasNodeOptions: boolean;
  hasInspector: boolean;
  noBrowserGlobals: boolean;
  bits: number;
  hasDtrace: boolean;
  getDefaultLocale(): string;
}
