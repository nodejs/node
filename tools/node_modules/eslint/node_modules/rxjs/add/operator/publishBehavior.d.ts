import { publishBehavior } from '../../operator/publishBehavior';
declare module '../../Observable' {
    interface Observable<T> {
        publishBehavior: typeof publishBehavior;
    }
}
