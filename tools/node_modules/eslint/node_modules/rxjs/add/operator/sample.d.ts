import { sample } from '../../operator/sample';
declare module '../../Observable' {
    interface Observable<T> {
        sample: typeof sample;
    }
}
