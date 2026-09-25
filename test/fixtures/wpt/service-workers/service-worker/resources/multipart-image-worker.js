importScripts('/common/get-host-info.sub.js');
importScripts('test-helpers.sub.js');

const host_info = get_host_info();

const multipart_image_path = base_path() + 'multipart-image.py';
const sameorigin_url = host_info['HTTPS_ORIGIN'] + multipart_image_path;
const cross_origin_url = host_info['HTTPS_REMOTE_ORIGIN'] + multipart_image_path;

self.addEventListener('fetch', event => {
    const url = event.request.url;
    if (url.indexOf('cross-origin-multipart-image-with-no-cors') >= 0) {
        event.respondWith(fetch(cross_origin_url, {mode: 'no-cors'}));
    } else if (url.indexOf('cross-origin-multipart-image-with-cors-rejected') >= 0) {
        event.respondWith(fetch(cross_origin_url, {mode: 'cors'}));
    } else if (url.indexOf('cross-origin-multipart-image-with-cors-approved') >= 0) {
        event.respondWith(fetch(cross_origin_url + '?approvecors', {mode: 'cors'}));
    } else if (url.indexOf('same-origin-multipart-image') >= 0) {
        event.respondWith(fetch(sameorigin_url));
    }
});
