import { audit } from '../../operator/audit';
declare module '../../Observable' {
    interface Observable<T> {
        audit: typeof audit;
    }
}
