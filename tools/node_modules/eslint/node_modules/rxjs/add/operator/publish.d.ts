import { publish } from '../../operator/publish';
declare module '../../Observable' {
    interface Observable<T> {
        publish: typeof publish;
    }
}
