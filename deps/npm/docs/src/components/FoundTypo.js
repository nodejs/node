import React from 'react'
import styled from 'styled-components'

const Container = styled.div`
  margin: 80px 0;
  border-top: 1px solid black;
  padding: 20px 0;
`

const FoundTypo = () => {
  return (
    <Container>
      <p><span role='img' aria-label='eyes-emoji'>👀</span> Found a typo? <a href='https://github.com/npm/cli/'>Let us know!</a></p>
      <p>The current stable version of npm is <a href='https://github.com/npm/cli/'>here</a>. To upgrade, run: <code className='language-text'>npm install npm@latest -g</code></p>
      <p>
        To report bugs or submit feature requests for the docs, please post <a href='https://npm.community/c/support/docs-needed'>here</a>.
        Submit npm issues <a href='https://npm.community/c/bugs'>here.</a>
      </p>
    </Container>
  )
}

export default FoundTypo
