const add_iframe_js = (iframe_origin, response_queue_uuid) => `
  const importScript = ${importScript};
  await importScript("/html/cross-origin-embedder-policy/credentialless" +
                   "/resources/common.js");
  await importScript("/html/anonymous-iframe/resources/common.js");
  await importScript("/common/utils.js");

  // dispatcher.js has already been loaded by the popup this is running in.
  await send("${response_queue_uuid}", newIframe("${iframe_origin}"));
`;

async function create_test_iframes(t, response_queue_uuid) {
  const same_site_origin = get_host_info().HTTPS_ORIGIN;
  const cross_site_origin = get_host_info().HTTPS_NOTSAMESITE_ORIGIN;

  assert_equals("https://" + window.location.host, same_site_origin,
  "this test assumes that the page's window.location.host corresponds to " +
  "get_host_info().HTTPS_ORIGIN");

  // Create a same-origin iframe in a cross-site popup.
  const not_same_site_popup_uuid = newPopup(t, cross_site_origin);
  await send(not_same_site_popup_uuid,
       add_iframe_js(same_site_origin, response_queue_uuid));
  const cross_site_iframe_uuid = await receive(response_queue_uuid);

  // Create a same-origin iframe in a same-site popup.
  const same_origin_popup_uuid = newPopup(t, same_site_origin);
  await send(same_origin_popup_uuid,
       add_iframe_js(same_site_origin, response_queue_uuid));
  const same_site_iframe_uuid = await receive(response_queue_uuid);

  return [cross_site_iframe_uuid, same_site_iframe_uuid];
}