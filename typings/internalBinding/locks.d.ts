declare namespace InternalLocksBinding {
  interface LockInfo {
    readonly name: string;
    readonly mode: 'shared' | 'exclusive';
    readonly clientId: string;
  }

  interface LockManagerSnapshot {
    readonly held: LockInfo[];
    readonly pending: LockInfo[];
  }

  type LockGrantedCallback = (lock: LockInfo | null) => Promise<any> | any;
}

export interface LocksBinding {
  readonly LOCK_MODE_SHARED: 'shared';
  readonly LOCK_MODE_EXCLUSIVE: 'exclusive';
  readonly LOCK_STOLEN_ERROR: 'LOCK_STOLEN';

  request(
    name: string,
    clientId: string,
    mode: string,
    steal: boolean,
    ifAvailable: boolean,
    callback: InternalLocksBinding.LockGrantedCallback
  ): Promise<any>;

  query(): Promise<InternalLocksBinding.LockManagerSnapshot>;
}
