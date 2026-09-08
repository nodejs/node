export interface MksnapshotBinding {
  runEmbedderPreload(process: object, require: Function): void;
  compileSerializeMain(
    filename: string,
    source: string,
  ): (
    require: (id: string) => unknown,
    __filename: string,
    __dirname: string,
  ) => unknown;
  setSerializeCallback(callback: () => void): void;
  setDeserializeCallback(callback: () => void): void;
  setDeserializeMainFunction(callback: () => unknown): void;
  anonymousMainPath: '__node_anonymous_main';
  isBuildingSnapshotBuffer: Uint8Array;
}
