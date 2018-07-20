import { Subject } from '../Subject';
import { Subscription } from '../Subscription';
import { SubscriptionLoggable } from './SubscriptionLoggable';
import { applyMixins } from '../util/applyMixins';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @ignore
 * @extends {Ignored}
 */
export class HotObservable extends Subject {
    constructor(messages, scheduler) {
        super();
        this.messages = messages;
        this.subscriptions = [];
        this.scheduler = scheduler;
    }
    /** @deprecated internal use only */ _subscribe(subscriber) {
        const subject = this;
        const index = subject.logSubscribedFrame();
        subscriber.add(new Subscription(() => {
            subject.logUnsubscribedFrame(index);
        }));
        return super._subscribe(subscriber);
    }
    setup() {
        const subject = this;
        const messagesLength = subject.messages.length;
        /* tslint:disable:no-var-keyword */
        for (var i = 0; i < messagesLength; i++) {
            (() => {
                var message = subject.messages[i];
                /* tslint:enable */
                subject.scheduler.schedule(() => { message.notification.observe(subject); }, message.frame);
            })();
        }
    }
}
applyMixins(HotObservable, [SubscriptionLoggable]);
//# sourceMappingURL=HotObservable.js.map