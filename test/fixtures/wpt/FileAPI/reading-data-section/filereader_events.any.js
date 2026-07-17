promise_test(async t => {
  var reader = new FileReader();
  var eventWatcher = new EventWatcher(t, reader, ['loadstart', 'progress', 'abort', 'error', 'load', 'loadend']);
  reader.readAsText(new Blob([]));
  await eventWatcher.wait_for('loadstart');
  // No progress event for an empty blob, as no data is loaded.
  await eventWatcher.wait_for('load');
  await eventWatcher.wait_for('loadend');
}, 'events are dispatched in the correct order for an empty blob');

promise_test(async t => {
  var reader = new FileReader();
  var eventWatcher = new EventWatcher(t, reader, ['loadstart', 'progress', 'abort', 'error', 'load', 'loadend']);
  reader.readAsText(new Blob(['a']));
  await eventWatcher.wait_for('loadstart');
  await eventWatcher.wait_for('progress');
  await eventWatcher.wait_for('load');
  await eventWatcher.wait_for('loadend');
}, 'events are dispatched in the correct order for a non-empty blob');
