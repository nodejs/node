export interface DiagnosticsChannelBinding {
  subscribers: Uint32Array;
  notifyChannelActive(index: number): void;
  notifyChannelInactive(index: number): void;
  linkNativeChannel(
    callback: (name: string, index: number) => object | undefined,
  ): void;
}
