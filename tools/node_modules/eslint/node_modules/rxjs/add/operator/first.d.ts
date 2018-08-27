import { first } from '../../operator/first';
declare module '../../Observable' {
    interface Observable<T> {
        first: typeof first;
    }
}
