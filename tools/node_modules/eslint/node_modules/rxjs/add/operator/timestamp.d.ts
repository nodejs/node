import { timestamp } from '../../operator/timestamp';
declare module '../../Observable' {
    interface Observable<T> {
        timestamp: typeof timestamp;
    }
}
