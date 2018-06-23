import { fromEventPattern as staticFromEventPattern } from '../../observable/fromEventPattern';
declare module '../../Observable' {
    namespace Observable {
        let fromEventPattern: typeof staticFromEventPattern;
    }
}
