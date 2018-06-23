import { delayWhen } from '../../operator/delayWhen';
declare module '../../Observable' {
    interface Observable<T> {
        delayWhen: typeof delayWhen;
    }
}
