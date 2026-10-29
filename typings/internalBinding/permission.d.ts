type Buffer = Uint8Array;

export interface PermissionBinding {
  has(scope: string, reference?: string | Buffer | null): boolean;
  drop(scope: string, reference?: string | Buffer | null): void;
}
