import { distinctUntilKeyChanged } from '../../operator/distinctUntilKeyChanged';
declare module '../../Observable' {
    interface Observable<T> {
        distinctUntilKeyChanged: typeof distinctUntilKeyChanged;
    }
}
