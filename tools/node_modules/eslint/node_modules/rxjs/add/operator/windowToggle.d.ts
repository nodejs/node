import { windowToggle } from '../../operator/windowToggle';
declare module '../../Observable' {
    interface Observable<T> {
        windowToggle: typeof windowToggle;
    }
}
