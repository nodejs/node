'use strict';
const common = require('../common.js');
const dc = require('diagnostics_channel');

// Benchmark to compare the performance of active/inactive channel state transitions
// and method dispatch overhead in diagnostics_channel

const bench = common.createBenchmark(main, {
  // Test scenarios
  scenario: [
    'inactive-only',      // Only create channels and publish to inactive channels
    'active-only',        // Only publish to active channels
    'transition-heavy',   // Many subscribe/unsubscribe transitions
    'mixed-workload',     // Mix of active and inactive channels with occasional transitions
  ],
  n: [1e5],              // Number of operations to perform
  channels: [1, 10, 100], // Number of distinct channels to use
  subs: [1, 5],          // Number of subscribers per active channel
});

function noop() {}

// Generate a deterministic sequence of channel names
function createChannelNames(count) {
  const names = [];
  for (let i = 0; i < count; i++) {
    names.push(`benchmark-channel-${i}`);
  }
  return names;
}

function main({ scenario, n, channels: channelCount, subs: subscriberCount }) {
  const channelNames = createChannelNames(channelCount);
  const channelObjects = channelNames.map((name) => dc.channel(name));
  const subscribers = [];

  // Create subscriber functions
  for (let i = 0; i < subscriberCount; i++) {
    subscribers.push(noop);
  }

  // Different test scenarios
  switch (scenario) {
    case 'inactive-only': {
      // Test publishing to inactive channels
      bench.start();
      for (let i = 0; i < n; i++) {
        const channel = channelObjects[i % channelCount];
        channel.publish({ data: 'test', iteration: i });
      }
      bench.end(n);
      break;
    }

    case 'active-only': {
      // First activate all channels
      for (let i = 0; i < channelCount; i++) {
        const channel = channelObjects[i];
        for (let j = 0; j < subscriberCount; j++) {
          channel.subscribe(subscribers[j]);
        }
      }

      // Test publishing to active channels
      bench.start();
      for (let i = 0; i < n; i++) {
        const channel = channelObjects[i % channelCount];
        channel.publish({ data: 'test', iteration: i });
      }
      bench.end(n);

      for (let i = 0; i < channelCount; i++) {
        const channel = channelObjects[i];
        for (let j = 0; j < subscriberCount; j++) {
          channel.unsubscribe(subscribers[j]);
        }
      }
      break;
    }

    case 'transition-heavy': {
      // Test rapidly switching between active and inactive states
      bench.start();
      for (let i = 0; i < n; i++) {
        const channelIndex = i % channelCount;
        const channel = channelObjects[channelIndex];

        // Every operation toggles a channel's state
        if (i % 2 === 0) {
          // Activate the channel
          channel.subscribe(subscribers[0]);
        } else {
          // Deactivate the channel
          channel.unsubscribe(subscribers[0]);
        }
      }
      bench.end(n);

      for (let i = 0; i < channelCount; i++) {
        channelObjects[i].unsubscribe(subscribers[0]);
      }
      break;
    }

    case 'mixed-workload': {
      // Mixed operations that mimic real-world usage patterns
      // Set up: activate some channels
      for (let i = 0; i < channelCount; i += 2) {
        const channel = channelObjects[i];
        for (let j = 0; j < subscriberCount; j++) {
          channel.subscribe(subscribers[j]);
        }
      }

      bench.start();
      for (let i = 0; i < n; i++) {
        const channelIndex = i % channelCount;
        const channel = channelObjects[channelIndex];
        const operation = i % 10; // Determine operation type

        if (operation < 6) {
          // 60% of operations are publish
          channel.publish({ data: 'test', iteration: i });
        } else if (operation < 8) {
          // 20% are hasSubscribers checks
          /* eslint-disable-next-line no-unused-vars */
          const hasSubscribers = channel.hasSubscribers;
        } else if (operation === 8) {
          // 10% are subscribe
          channel.subscribe(subscribers[i % subscriberCount]);
        } else {
          // 10% are unsubscribe
          channel.unsubscribe(subscribers[i % subscriberCount]);
        }
      }
      bench.end(n);

      for (let i = 0; i < channelCount; i++) {
        const channel = channelObjects[i];
        for (let j = 0; j < subscriberCount; j++) {
          channel.unsubscribe(subscribers[j]);
        }
      }
      break;
    }
  }
}
