import { retryWhen } from '../../operator/retryWhen';
declare module '../../Observable' {
    interface Observable<T> {
        retryWhen: typeof retryWhen;
    }
}
