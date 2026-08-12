export interface CredentialsBinding {
  implementsPosixCredentials?: true;
  safeGetenv(key: string): string | undefined;
  getTempDir(): string | undefined;

  getuid?(): number;
  geteuid?(): number;
  getgid?(): number;
  getegid?(): number;
  getgroups?(): number[];

  initgroups?(user: string | number, extraGroup: string | number): 0 | 1 | 2;
  setegid?(id: string | number): 0 | 1;
  seteuid?(id: string | number): 0 | 1;
  setgid?(id: string | number): 0 | 1;
  setuid?(id: string | number): 0 | 1;
  setgroups?(groups: Array<string | number>): number;
}
