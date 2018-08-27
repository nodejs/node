import { windowWhen } from '../../operator/windowWhen';
declare module '../../Observable' {
    interface Observable<T> {
        windowWhen: typeof windowWhen;
    }
}
