import { combineLatest } from '../../operator/combineLatest';
declare module '../../Observable' {
    interface Observable<T> {
        combineLatest: typeof combineLatest;
    }
}
