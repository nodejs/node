import { switchMapTo } from '../../operator/switchMapTo';
declare module '../../Observable' {
    interface Observable<T> {
        switchMapTo: typeof switchMapTo;
    }
}
