import { take } from '../../operator/take';
declare module '../../Observable' {
    interface Observable<T> {
        take: typeof take;
    }
}
