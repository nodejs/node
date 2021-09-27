declare function InternalBinding(binding: 'os'): {
  getHostname(ctx: {}): string | undefined;
  getLoadAvg(array: Float64Array): void;
  getUptime(): number;
  getTotalMem(): number;
  getFreeMem(): number;
  getCPUs(): Array<string | number>;
  getInterfaceAddresses(ctx: {}): Array<string | number | boolean> | undefined;
  getHomeDirectory(ctx: {}): string | undefined;
  getUserInfo(options: { encoding?: string } | undefined, ctx: {}): {
    uid: number;
    gid: number;
    username: string;
    homedir: string;
    shell: string | null;
  } | undefined;
  setPriority(pid: number, priority: number, ctx: {}): number;
  getPriority(pid: number, ctx: {}): number | undefined;
  getOSInformation(ctx: {}): [sysname: string, version: string, release: string];
  isBigEndian: boolean;
};
