import { startWith } from '../../operator/startWith';
declare module '../../Observable' {
    interface Observable<T> {
        startWith: typeof startWith;
    }
}
