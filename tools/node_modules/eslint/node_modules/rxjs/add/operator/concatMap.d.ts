import { concatMap } from '../../operator/concatMap';
declare module '../../Observable' {
    interface Observable<T> {
        concatMap: typeof concatMap;
    }
}
