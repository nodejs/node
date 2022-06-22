test(t => {
  const hostParent = document.createElement("section"),
        host = hostParent.appendChild(document.createElement("div")),
        shadowRoot = host.attachShadow({ mode: "closed" }),
        targetParent = shadowRoot.appendChild(document.createElement("p")),
        target = targetParent.appendChild(document.createElement("span")),
        path = [hostParent, host, shadowRoot, targetParent, target],
        expected = [],
        result = [];
  path.forEach((node, index) => {
    expected.splice(index, 0, "capturing " + node.nodeName);
    expected.splice(index + 1, 0, "bubbling " + node.nodeName);
  });
  path.forEach(node => {
    node.addEventListener("test", () => { result.push("bubbling " + node.nodeName) });
    node.addEventListener("test", () => { result.push("capturing " + node.nodeName) }, true);
  });
  target.dispatchEvent(new CustomEvent('test', { detail: {}, bubbles: true, composed: true }));
  assert_array_equals(result, expected);
});
