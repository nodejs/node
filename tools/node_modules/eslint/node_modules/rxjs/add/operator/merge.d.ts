import { merge } from '../../operator/merge';
declare module '../../Observable' {
    interface Observable<T> {
        merge: typeof merge;
    }
}
