import { switchMap } from '../../operator/switchMap';
declare module '../../Observable' {
    interface Observable<T> {
        switchMap: typeof switchMap;
    }
}
