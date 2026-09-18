type SpawnSyncStdio =
  | { type: 'ignore' }
  | { type: 'pipe'; readable: boolean; writable: boolean; input?: Uint8Array }
  | { type: 'inherit'; fd: number }
  | { type: 'fd'; fd: number };

export interface SpawnSyncOptions {
  file: string;
  args: readonly string[];
  cwd?: string | null;
  envPairs?: readonly string[] | null;
  uid?: number;
  gid?: number;
  detached?: boolean;
  windowsHide?: boolean;
  windowsVerbatimArguments?: boolean;
  timeout?: number;
  maxBuffer?: number;
  killSignal?: number;
  stdio: readonly SpawnSyncStdio[];
}

export interface SpawnSyncResult {
  error?: number;
  status: number | null;
  signal: string | null;
  output: (Uint8Array | null)[] | null;
  pid: number;
}

export interface SpawnSyncBinding {
  spawn(options: SpawnSyncOptions): SpawnSyncResult;
}
