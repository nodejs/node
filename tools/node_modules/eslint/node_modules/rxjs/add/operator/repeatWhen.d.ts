import { repeatWhen } from '../../operator/repeatWhen';
declare module '../../Observable' {
    interface Observable<T> {
        repeatWhen: typeof repeatWhen;
    }
}
