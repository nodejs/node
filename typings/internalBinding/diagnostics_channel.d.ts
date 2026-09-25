export interface DiagnosticsChannelBinding {
  subscribers: Uint32Array;
  linkNativeChannel(
    callback: (name: string, index: number) => object | undefined,
  ): void;
}
