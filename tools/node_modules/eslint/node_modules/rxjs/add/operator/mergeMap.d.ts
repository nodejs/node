import { mergeMap } from '../../operator/mergeMap';
declare module '../../Observable' {
    interface Observable<T> {
        flatMap: typeof mergeMap;
        mergeMap: typeof mergeMap;
    }
}
