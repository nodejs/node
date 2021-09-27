let eqIsh = (a, b, fuzz = 2) => {
    return (Math.abs(a - b) <= fuzz);
  };

  let rectNotEQ = (a, b) => {
    return (!eqIsh(a.width, b.width) ||
      !eqIsh(a.height, b.height));
  };

  let spaced = new WeakMap();

  let reserveSpace = (el, rect = el.getClientBoundingRect()) => {
    let old = spaced.get(el);
    if (!old || rectNotEQ(old, rect)) {
      spaced.set(el, rect);
      el.attributeStyleMap.set(
        "contain-intrinsic-size",
        `${rect.width}px ${rect.height}px`
      );
    }
  };

  let iObs = new IntersectionObserver(
    (entries, o) => {
      entries.forEach((entry) => {

        reserveSpace(entry.target,
          entry.boundingClientRect);
      });
    },
    { rootMargin: "500px 0px 500px 0px" }
  );

  let rObs = new ResizeObserver(
    (entries, o) => {
      entries.forEach((entry) => {
        reserveSpace(entry.target, entry.contentRect);
      });
    }
  );

  let articles =
    document.querySelectorAll("#apicontent>section");

  articles.forEach((el) => {
    iObs.observe(el);
    rObs.observe(el);
  });

  requestAnimationFrame(() => {
    requestAnimationFrame(() => {
      articles[0].attributeStyleMap.set(
        "content-visibility",
        "auto"
      );
    });
  });