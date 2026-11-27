declare namespace InternalWebstorageBinding {
  class Storage {
    constructor(constructorKey: symbol, location: string);
    readonly length: number;
    clear(): void;
    getItem(key: string): string | null;
    key(index: number): string | null;
    removeItem(key: string): void;
    setItem(key: string, value: string): void;
  }
}

export interface WebstorageBinding {
  Storage: typeof InternalWebstorageBinding.Storage;
  kConstructorKey: symbol;
}
