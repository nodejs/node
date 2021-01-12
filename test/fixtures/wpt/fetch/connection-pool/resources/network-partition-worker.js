// This tests the partition key of fetches to subresouce_origin made by the worker and
// imported scripts from subresource_origin.
importScripts('SUBRESOURCE_PREFIX:&dispatch=fetch_file&path=common/utils.js');
importScripts('SUBRESOURCE_PREFIX:&dispatch=fetch_file&path=resources/testharness.js');
importScripts('SUBRESOURCE_PREFIX:&dispatch=fetch_file&path=fetch/connection-pool/resources/network-partition-key.js');

async function fetch_and_reply() {
  try {
     await check_partition_ids();
     self.postMessage({result: 'success'});
  } catch (e) {
    self.postMessage({result: 'error', details: e.message});
  }
}
fetch_and_reply();
