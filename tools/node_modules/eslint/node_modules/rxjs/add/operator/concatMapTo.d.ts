import { concatMapTo } from '../../operator/concatMapTo';
declare module '../../Observable' {
    interface Observable<T> {
        concatMapTo: typeof concatMapTo;
    }
}
