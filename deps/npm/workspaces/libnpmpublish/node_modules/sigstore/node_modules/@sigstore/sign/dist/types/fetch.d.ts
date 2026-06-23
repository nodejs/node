import type { MakeFetchHappenOptions } from 'make-fetch-happen';
export type Retry = MakeFetchHappenOptions['retry'];
export type FetchOptions = {
    retry?: Retry;
    timeout?: number | undefined;
};
