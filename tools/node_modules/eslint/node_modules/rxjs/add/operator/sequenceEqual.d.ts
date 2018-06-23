import { sequenceEqual } from '../../operator/sequenceEqual';
declare module '../../Observable' {
    interface Observable<T> {
        sequenceEqual: typeof sequenceEqual;
    }
}
