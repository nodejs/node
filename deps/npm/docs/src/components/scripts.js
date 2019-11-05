import React from 'react'

const IS_STATIC = process.env.GATSBY_IS_STATIC

const Scripts = () => {
  if (IS_STATIC) {
    return (
      <script
        dangerouslySetInnerHTML={{
          __html: `
          // Workaround: Make links on our Markdown files work on the static site
          var anchors = document.querySelectorAll("#see-also + ul li a")
          Array.prototype.slice.call(anchors).map(function(el) { el.href = el + "/index.html" })
          `
        }}
      />
    )
  }

  return null
}

export default Scripts
