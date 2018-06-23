import { min } from '../../operator/min';
declare module '../../Observable' {
    interface Observable<T> {
        min: typeof min;
    }
}
