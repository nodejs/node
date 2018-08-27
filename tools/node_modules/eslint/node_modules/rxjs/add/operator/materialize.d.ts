import { materialize } from '../../operator/materialize';
declare module '../../Observable' {
    interface Observable<T> {
        materialize: typeof materialize;
    }
}
