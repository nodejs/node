import { observeOn } from '../../operator/observeOn';
declare module '../../Observable' {
    interface Observable<T> {
        observeOn: typeof observeOn;
    }
}
