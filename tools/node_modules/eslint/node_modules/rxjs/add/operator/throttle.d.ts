import { throttle } from '../../operator/throttle';
declare module '../../Observable' {
    interface Observable<T> {
        throttle: typeof throttle;
    }
}
