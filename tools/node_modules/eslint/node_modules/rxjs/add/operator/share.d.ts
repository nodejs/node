import { share } from '../../operator/share';
declare module '../../Observable' {
    interface Observable<T> {
        share: typeof share;
    }
}
