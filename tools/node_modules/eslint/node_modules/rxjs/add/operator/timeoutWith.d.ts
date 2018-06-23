import { timeoutWith } from '../../operator/timeoutWith';
declare module '../../Observable' {
    interface Observable<T> {
        timeoutWith: typeof timeoutWith;
    }
}
