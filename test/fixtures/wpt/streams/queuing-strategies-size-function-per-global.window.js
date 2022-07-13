const iframe = document.createElement('iframe');
document.body.appendChild(iframe);

for (const type of ['CountQueuingStrategy', 'ByteLengthQueuingStrategy']) {
  test(() => {
    const myQs = new window[type]({ highWaterMark: 1 });
    const yourQs = new iframe.contentWindow[type]({ highWaterMark: 1 });
    assert_not_equals(myQs.size, yourQs.size,
                      'size should not be the same object');
  }, `${type} size should be different for objects in different realms`);
}

// Cleanup the document to avoid messing up the result page.
iframe.remove();
