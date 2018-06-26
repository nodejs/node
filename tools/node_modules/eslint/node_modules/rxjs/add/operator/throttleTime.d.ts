import { throttleTime } from '../../operator/throttleTime';
declare module '../../Observable' {
    interface Observable<T> {
        throttleTime: typeof throttleTime;
    }
}
