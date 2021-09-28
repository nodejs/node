'use strict';

{
  const eqIsh = (a, b, fuzz = 2) => {
    return (Math.abs(a - b) <= fuzz);
  };

  const rectNotEQ = (a, b) => {
    return (!eqIsh(a.width, b.width) ||
      !eqIsh(a.height, b.height));
  };

  const spaced = new WeakMap();

  const reserveSpace = (el, rect = el.getClientBoundingRect()) => {
    const old = spaced.get(el);
    if (!old || rectNotEQ(old, rect)) {
      spaced.set(el, rect);
      el.attributeStyleMap.set(
        'contain-intrinsic-size',
        `${rect.width}px ${rect.height}px`
      );
    }
  };

  const iObs = new IntersectionObserver(
    (entries, o) => {
      entries.forEach((entry) => {

        reserveSpace(entry.target,
                     entry.boundingClientRect);
      });
    },
    { rootMargin: '500px 0px 500px 0px' }
  );

  const rObs = new ResizeObserver(
    (entries, o) => {
      entries.forEach((entry) => {
        reserveSpace(entry.target, entry.contentRect);
      });
    }
  );

  const articles =
    document.querySelectorAll('#apicontent>section');

  articles.forEach((el) => {
    iObs.observe(el);
    rObs.observe(el);
  });

  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      articles[0].attributeStyleMap.set(
        'content-visibility',
        'auto'
      );
    });
  });
}
