import React from 'react'
import styled from 'styled-components'
import DocLinks from './DocLinks'
import {} from '../components/Sidebar'

const MobileContainer = styled.div`
  border-left: 1px solid #86838333;
  border-bottom: 1px solid #86838333;
  padding: 30px 30px 200px;
  width: 340px;
  display: block;
  height: calc(100vh - 54px);
  overflow: scroll;
  position: fixed;
  top: 54px;
  right: 0px;
  background-color: ${(props) => props.theme.colors.white};
  z-index: 100;
 
  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    display: none;
  } 
`

const MobileSidebar = () => {
  return (
    <MobileContainer flexDirection='column'>
      <DocLinks />
    </MobileContainer>
  )
}

export default MobileSidebar
