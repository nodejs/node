declare namespace InternalBlobBinding {
  interface BlobHandle {
    slice(start: number, end: number): BlobHandle;
  }
}

export interface BlobBinding {
  createBlob(sources: Array<Uint8Array | InternalBlobBinding.BlobHandle>, length: number): InternalBlobBinding.BlobHandle;
  getDataObject(id: string): [handle: InternalBlobBinding.BlobHandle | undefined, length: number, type: string] | undefined;
  storeDataObject(id: string, handle: InternalBlobBinding.BlobHandle, size: number, type: string): void;
}
