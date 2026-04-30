declare namespace InternalOSBinding {
  type OSContext = {};
}

export interface OSBinding {
  getHostname(ctx: InternalOSBinding.OSContext): string | undefined;
  getLoadAvg(array: Float64Array): void;
  getUptime(): number;
  getTotalMem(): number;
  getFreeMem(): number;
  getCPUs(): Array<string | number>;
  getInterfaceAddresses(ctx: InternalOSBinding.OSContext): Array<string | number | boolean> | undefined;
  getHomeDirectory(ctx: InternalOSBinding.OSContext): string | undefined;
  getUserInfo(options: { encoding?: string } | undefined, ctx: InternalOSBinding.OSContext): {
    uid: number;
    gid: number;
    username: string;
    homedir: string;
    shell: string | null;
  } | undefined;
  setPriority(pid: number, priority: number, ctx: InternalOSBinding.OSContext): number;
  getPriority(pid: number, ctx: InternalOSBinding.OSContext): number | undefined;
  getOSInformation(ctx: InternalOSBinding.OSContext): [sysname: string, version: string, release: string];
  isBigEndian: boolean;
  getAvailableParallelism(): number;
}
