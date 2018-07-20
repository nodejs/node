import { mergeMapTo } from '../../operator/mergeMapTo';
declare module '../../Observable' {
    interface Observable<T> {
        flatMapTo: typeof mergeMapTo;
        mergeMapTo: typeof mergeMapTo;
    }
}
