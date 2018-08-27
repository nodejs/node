import { from as staticFrom } from '../../observable/from';
declare module '../../Observable' {
    namespace Observable {
        let from: typeof staticFrom;
    }
}
