type Buffer = Uint8Array;

export interface CryptoBinding {
  SecureContext: new () => unknown;
  timingSafeEqual: (a: Buffer | ArrayBuffer | TypedArray | DataView, b: Buffer | ArrayBuffer | TypedArray | DataView) => boolean;
  getFipsCrypto(): boolean;
  setFipsCrypto(bool: boolean): void;
  getSSLCiphers(): unknown;
  getRootCertificates(): unknown;
}