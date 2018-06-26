import { pairwise } from '../../operator/pairwise';
declare module '../../Observable' {
    interface Observable<T> {
        pairwise: typeof pairwise;
    }
}
