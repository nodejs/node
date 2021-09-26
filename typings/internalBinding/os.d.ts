declare function InternalBinding(binding: 'os'): {
  getHostname(ctx: object): string | undefined;
  getLoadAvg(array: Float64Array): void;
  getUptime(): number;
  getTotalMem(): number;
  getFreeMem(): number;
  getCPUs(): Array<string | number>;
  getInterfaceAddresses(ctx: object): Array<string | number | boolean> | undefined;
  getHomeDirectory(ctx: object): string | undefined;
  getUserInfo(options: {encoding?: string} | undefined, ctx: object): {
    uid: number;
    gid: number;
    username: string;
    homedir: string;
    shell: string | null;
  } | undefined;
  setPriority(pid: number, priority: number, ctx: object): number;
  getPriority(pid: number, ctx: object): number | undefined;
  getOSInformation(ctx: object): [sysname: string, version: string, release: string];
  isBigEndian: boolean;
};
