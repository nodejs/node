import { windowTime } from '../../operator/windowTime';
declare module '../../Observable' {
    interface Observable<T> {
        windowTime: typeof windowTime;
    }
}
