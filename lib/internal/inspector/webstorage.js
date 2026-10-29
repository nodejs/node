'use strict';

const { Storage } = internalBinding('webstorage');
const { DOMStorage } = require('inspector');
const path = require('path');
const { getOptionValue } = require('internal/options');

class InspectorLocalStorage extends Storage {
  setItem(key, value) {
    key = `${key}`;
    value = `${value}`;
    const oldValue = this.getItem(key);
    super.setItem(key, value);
    if (oldValue == null) {
      itemAdded(key, value, true);
    } else {
      itemUpdated(key, oldValue, value, true);
    }
  }

  removeItem(key) {
    key = `${key}`;
    super.removeItem(key);
    itemRemoved(key, true);
  }

  clear() {
    super.clear();
    itemsCleared(true);
  }
}

const InspectorSessionStorage = class extends Storage {
  setItem(key, value) {
    key = `${key}`;
    value = `${value}`;
    const oldValue = this.getItem(key);
    super.setItem(key, value);
    if (oldValue == null) {
      itemAdded(key, value, false);
    } else {
      itemUpdated(key, oldValue, value, false);
    }
  }

  removeItem(key) {
    key = `${key}`;
    super.removeItem(key);
    itemRemoved(key, false);
  }

  clear() {
    super.clear();
    itemsCleared(false);
  }
};

function itemAdded(key, value, isLocalStorage) {
  DOMStorage.domStorageItemAdded({
    key,
    newValue: value,
    storageId: {
      securityOrigin: '',
      isLocalStorage,
      storageKey: getStorageKey(),
    },
  });
}

function itemUpdated(key, oldValue, newValue, isLocalStorage) {
  DOMStorage.domStorageItemUpdated({
    key,
    oldValue,
    newValue,
    storageId: {
      securityOrigin: '',
      isLocalStorage,
      storageKey: getStorageKey(),
    },
  });
}

function itemRemoved(key, isLocalStorage) {
  DOMStorage.domStorageItemRemoved({
    key,
    storageId: {
      securityOrigin: '',
      isLocalStorage,
      storageKey: getStorageKey(),
    },
  });
}

function itemsCleared(isLocalStorage) {
  DOMStorage.domStorageItemsCleared({
    storageId: {
      securityOrigin: '',
      isLocalStorage,
      storageKey: getStorageKey(),
    },
  });
}

function getStorageKey() {
  const localStorageFile = getOptionValue('--localstorage-file');
  const resolvedAbsolutePath = path.resolve(localStorageFile);
  return 'file://' + resolvedAbsolutePath;
}

module.exports = {
  InspectorLocalStorage,
  InspectorSessionStorage,
};
