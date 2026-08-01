// This worker imports a script that returns 200 on the first request and 404
// on the second request, and a script that is updated every time when
// requesting it.
const params = new URLSearchParams(location.search);
const key = params.get('Key');
const additional_key = params.get('AdditionalKey');
importScripts(`update-worker.py?Key=${key}&Mode=not_found`,
              `update-worker.py?Key=${additional_key}&Mode=normal`);
