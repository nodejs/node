import { using as staticUsing } from '../../observable/using';
declare module '../../Observable' {
    namespace Observable {
        let using: typeof staticUsing;
    }
}
