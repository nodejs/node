import { shareReplay } from '../../operator/shareReplay';
declare module '../../Observable' {
    interface Observable<T> {
        shareReplay: typeof shareReplay;
    }
}
