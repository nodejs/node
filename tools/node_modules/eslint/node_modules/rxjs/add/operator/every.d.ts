import { every } from '../../operator/every';
declare module '../../Observable' {
    interface Observable<T> {
        every: typeof every;
    }
}
