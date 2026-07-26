export interface ConfigBinding {
  isDebugBuild: boolean;
  openSSLIsBoringSSL: boolean;
  hasOpenSSL: boolean;
  hasIntl: boolean;
  hasSmallICU: boolean;
  hasTracing: boolean;
  hasNodeOptions: boolean;
  hasInspector: boolean;
  hasSQLite: boolean;
  noBrowserGlobals: boolean;
  bits: number;
  getDefaultLocale(): string;
}
