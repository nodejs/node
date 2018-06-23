import { timeout } from '../../operator/timeout';
declare module '../../Observable' {
    interface Observable<T> {
        timeout: typeof timeout;
    }
}
