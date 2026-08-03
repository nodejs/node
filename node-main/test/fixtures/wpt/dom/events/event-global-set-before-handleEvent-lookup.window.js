// https://dom.spec.whatwg.org/#concept-event-listener-inner-invoke (steps 8.2 - 12)
// https://webidl.spec.whatwg.org/#call-a-user-objects-operation (step 10.1)

test(() => {
  const eventTarget = new EventTarget;

  let currentEvent;
  eventTarget.addEventListener("foo", {
    get handleEvent() {
      currentEvent = window.event;
      return () => {};
    }
  });

  const event = new Event("foo");
  eventTarget.dispatchEvent(event);

  assert_equals(currentEvent, event);
}, "window.event is set before 'handleEvent' lookup");
