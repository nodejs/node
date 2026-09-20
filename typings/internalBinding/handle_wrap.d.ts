import type { AsyncWrap } from './async_wrap';

export interface HandleWrap extends AsyncWrap {
  close(callback?: () => void): void;
  hasRef(): boolean;
  ref(): void;
  unref(): void;
}
