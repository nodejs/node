'use strict';

const {
  TraceSigintWatchdog
} = internalBinding('watchdog');

class SigintWatchdog extends TraceSigintWatchdog {
  #started = false;
  #effective = false;
  #onNewListener = (eve) => {
    if (eve === 'SIGINT' && this.#effective) {
      super.stop();
      this.#effective = false;
    }
  };
  #onRemoveListener = (eve) => {
    if (eve === 'SIGINT' && process.listenerCount('SIGINT') === 0 &&
        !this.#effective) {
      super.start();
      this.#effective = true;
    }
  };

  start() {
    if (this.#started) {
      return;
    }
    this.#started = true;
    // Prepend sigint newListener to remove stop watchdog before signal wrap
    // been activated. Also make sigint removeListener been ran after signal
    // wrap been stopped.
    process.prependListener('newListener', this.#onNewListener);
    process.addListener('removeListener', this.#onRemoveListener);

    if (process.listenerCount('SIGINT') === 0) {
      super.start();
      this.#effective = true;
    }
  }

  stop() {
    if (!this.#started) {
      return;
    }
    this.#started = false;
    process.removeListener('newListener', this.#onNewListener);
    process.removeListener('removeListener', this.#onRemoveListener);

    if (this.#effective) {
      super.stop();
      this.#effective = false;
    }
  }
}


module.exports = {
  SigintWatchdog
};
