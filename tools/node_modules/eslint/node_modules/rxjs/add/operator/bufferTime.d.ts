import { bufferTime } from '../../operator/bufferTime';
declare module '../../Observable' {
    interface Observable<T> {
        bufferTime: typeof bufferTime;
    }
}
