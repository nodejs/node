import { retry } from '../../operator/retry';
declare module '../../Observable' {
    interface Observable<T> {
        retry: typeof retry;
    }
}
