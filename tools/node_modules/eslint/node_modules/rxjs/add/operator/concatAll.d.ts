import { concatAll } from '../../operator/concatAll';
declare module '../../Observable' {
    interface Observable<T> {
        concatAll: typeof concatAll;
    }
}
