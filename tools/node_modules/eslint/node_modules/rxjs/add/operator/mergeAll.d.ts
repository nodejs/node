import { mergeAll } from '../../operator/mergeAll';
declare module '../../Observable' {
    interface Observable<T> {
        mergeAll: typeof mergeAll;
    }
}
