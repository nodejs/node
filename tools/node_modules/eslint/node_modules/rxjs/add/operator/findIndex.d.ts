import { findIndex } from '../../operator/findIndex';
declare module '../../Observable' {
    interface Observable<T> {
        findIndex: typeof findIndex;
    }
}
