import { Observable } from '../Observable';
import { subscribeToResult } from '../util/subscribeToResult';
import { OuterSubscriber } from '../OuterSubscriber';
/**
 * We need this JSDoc comment for affecting ESDoc.
 * @extends {Ignored}
 * @hide true
 */
export class UsingObservable extends Observable {
    constructor(resourceFactory, observableFactory) {
        super();
        this.resourceFactory = resourceFactory;
        this.observableFactory = observableFactory;
    }
    static create(resourceFactory, observableFactory) {
        return new UsingObservable(resourceFactory, observableFactory);
    }
    /** @deprecated internal use only */ _subscribe(subscriber) {
        const { resourceFactory, observableFactory } = this;
        let resource;
        try {
            resource = resourceFactory();
            return new UsingSubscriber(subscriber, resource, observableFactory);
        }
        catch (err) {
            subscriber.error(err);
        }
    }
}
class UsingSubscriber extends OuterSubscriber {
    constructor(destination, resource, observableFactory) {
        super(destination);
        this.resource = resource;
        this.observableFactory = observableFactory;
        destination.add(resource);
        this.tryUse();
    }
    tryUse() {
        try {
            const source = this.observableFactory.call(this, this.resource);
            if (source) {
                this.add(subscribeToResult(this, source));
            }
        }
        catch (err) {
            this._error(err);
        }
    }
}
//# sourceMappingURL=UsingObservable.js.map