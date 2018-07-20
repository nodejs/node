import { onErrorResumeNext } from '../../operator/onErrorResumeNext';
declare module '../../Observable' {
    interface Observable<T> {
        onErrorResumeNext: typeof onErrorResumeNext;
    }
}
