import { pairs as staticPairs } from '../../observable/pairs';
declare module '../../Observable' {
    namespace Observable {
        let pairs: typeof staticPairs;
    }
}
