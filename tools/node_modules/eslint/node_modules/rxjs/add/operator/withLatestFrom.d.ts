import { withLatestFrom } from '../../operator/withLatestFrom';
declare module '../../Observable' {
    interface Observable<T> {
        withLatestFrom: typeof withLatestFrom;
    }
}
