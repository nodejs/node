import React from 'react'

const IS_STATIC = process.env.GATSBY_IS_STATIC

const Scripts = () => {
  // Workaround: Make links work on the static html site
  if (IS_STATIC) {
    return (
      <script
        dangerouslySetInnerHTML={{
          __html: `
          var anchors = document.querySelectorAll(".sidebar a, .documentation a")
          Array.prototype.slice.call(anchors).map(function(el) {
            if (el.href.match(/file:\\/\\//)) {
              el.href = el.href + "/index.html"
            }
          })
          `
        }}
      />
    )
  }

  return null
}

export default Scripts
