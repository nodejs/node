import { timeInterval } from '../../operator/timeInterval';
declare module '../../Observable' {
    interface Observable<T> {
        timeInterval: typeof timeInterval;
    }
}
