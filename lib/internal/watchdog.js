'use strict';

const {
  TraceSigintWatchdog,
} = internalBinding('watchdog');

class SigintWatchdog extends TraceSigintWatchdog {
  _started = false;
  _effective = false;
  _onNewListener = (eve) => {
    if (eve === 'SIGINT' && this._effective) {
      super.stop();
      this._effective = false;
    }
  };
  _onRemoveListener = (eve) => {
    if (eve === 'SIGINT' && process.listenerCount('SIGINT') === 0 &&
        !this._effective) {
      super.start();
      this._effective = true;
    }
  };

  start() {
    if (this._started) {
      return;
    }
    this._started = true;
    // Prepend sigint newListener to remove stop watchdog before signal wrap
    // been activated. Also make sigint removeListener been ran after signal
    // wrap been stopped.
    process.prependListener('newListener', this._onNewListener);
    process.addListener('removeListener', this._onRemoveListener);

    if (process.listenerCount('SIGINT') === 0) {
      super.start();
      this._effective = true;
    }
  }

  stop() {
    if (!this._started) {
      return;
    }
    this._started = false;
    process.removeListener('newListener', this._onNewListener);
    process.removeListener('removeListener', this._onRemoveListener);

    if (this._effective) {
      super.stop();
      this._effective = false;
    }
  }
}


module.exports = {
  SigintWatchdog,
};
