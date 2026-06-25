import { owner_symbol } from './symbols';

declare namespace InternalProcessWrapBinding {
  type StdioType = 'ignore' | 'pipe' | 'overlapped' | 'wrap' | 'inherit' | 'fd';

  interface StdioContainer {
    type: StdioType;
    handle?: object;
    fd?: number;
  }

  class Process {
    constructor();
    [owner_symbol]?: object;
    pid: number;
    onexit: (exitCode: number, signalCode: string) => void;
    spawn(
      file: string,
      args: string[] | undefined,
      cwd: string | undefined,
      envPairs: string[] | undefined,
      stdio: StdioContainer[],
      flags: number,
      uid: number | null | undefined,
      gid: number | null | undefined,
    ): number;
    kill(signal: number): number;
    ref(): void;
    unref(): void;
    close(callback?: () => void): void;
  }

  interface ProcessConstants {
    kProcessFlagDetached: number;
    kProcessFlagWindowsHide: number;
    kProcessFlagWindowsVerbatimArguments: number;
  }
}

export interface ProcessWrapBinding {
  Process: typeof InternalProcessWrapBinding.Process;
  constants: InternalProcessWrapBinding.ProcessConstants;
}
