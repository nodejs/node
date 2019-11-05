import React from 'react'
import styled from 'styled-components'
import DocLinks from './DocLinks'

const Container = styled.nav`
  border-right: 1px solid #86838333;
  padding: 30px;
  height: 100vh;
  display: none;
  width: 380px;
  position: sticky;
  overflow: scroll; 
  padding-bottom: 200px;
  top: 54px;
  background-color: ${(props) => props.theme.colors.white};

  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    display: block;
  } 
`

const Sidebar = () => {
  return (
    <Container>
      <DocLinks />
    </Container>
  )
}

export default Sidebar
