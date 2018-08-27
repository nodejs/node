import { sampleTime } from '../../operator/sampleTime';
declare module '../../Observable' {
    interface Observable<T> {
        sampleTime: typeof sampleTime;
    }
}
