import { never as staticNever } from '../../observable/never';
declare module '../../Observable' {
    namespace Observable {
        let never: typeof staticNever;
    }
}
