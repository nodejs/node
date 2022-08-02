"use strict";
importScripts("/resources/testharness.js");
importScripts("./test-incrementer.js");

self.onmessage = ({ data }) => {
  // data will be a MessagePort
  setupDestinationIncrementer(data, data);
};
