export interface SeaBinding {
  getAsset(key: string): ArrayBuffer | undefined;
  isExperimentalSeaWarningNeeded(): boolean;
  isSea(): boolean;
}
