/*
 * Polyfill for attaching shadow trees for declarative Shadow DOM for implementations that do not support
 * declarative Shadow DOM.
 *
 * root: The root of the subtree to perform the attachments in
 */

function polyfill_declarative_shadow_dom(root) {
  root.querySelectorAll("template[shadowroot]").forEach(template => {
    const mode = template.getAttribute("shadowroot");
    const shadowRoot = template.parentNode.attachShadow({ mode });
    shadowRoot.appendChild(template.content);
    template.remove();
    polyfill_declarative_shadow_dom(shadowRoot);
  });
}
