export interface SeaBinding {
  getAsset(key: string): ArrayBuffer | undefined;
  getAssetKeys(): string[];
  isExperimentalSeaWarningNeeded(): boolean;
  isSea(): boolean;
  isVfsEnabled(): boolean;
  mainCodePath?: string;
}
