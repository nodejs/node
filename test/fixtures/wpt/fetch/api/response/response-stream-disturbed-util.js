const BODY = '{"key": "value"}';

function responseFromBodySource(bodySource) {
  if (bodySource === "fetch") {
    return fetch("../resources/data.json");
  } else if (bodySource === "stream") {
    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(new TextEncoder().encode(BODY));
        controller.close();
      },
    });
    return new Response(stream);
  } else {
    return new Response(BODY);
  }
}
