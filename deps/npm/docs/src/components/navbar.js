import React from 'react'
import styled from 'styled-components'
import {Flex, Image, Box} from 'rebass'
import cliLogo from '../images/cli-logo.svg'
import {Link} from 'gatsby'
import {NavLink, BasicNavLink} from './links'
import MobileSidebar from '../components/MobileSidebar'
import hamburger from '../images/hamburger.svg'
import hamburgerClose from '../images/hamburger-close.svg'

const IS_STATIC = !!process.env.GATSBY_IS_STATIC

const Container = styled(Flex)`
  width: 100%;
  border-bottom: 1px solid #86838333;
  position: sticky;
  top: 0;
  background-color: ${(props) => props.theme.colors.white};
  z-index: 1;
`

const Inner = styled(Flex)`
  border-top: 3px solid;
  border-image: linear-gradient(139deg, #fb8817, #ff4b01, #c12127, #e02aff) 3;
  margin: auto;
  height: 53px;
  padding: 0 30px;
  align-items: center;
  width: 100%;
`

const Logo = styled(Image)`
  width: 120px;
  padding: 0px 5px;
  height: 18px;
  vertical-align: middle;
  display: inline-block;
  transition: opacity .5s;

  &:hover {
    opacity: .8;
  }
`

const Links = styled.ul`
  display: none;

  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    display: block;
    margin-left: auto;
  }
`

const Heart = styled(Box)`
  font-size: 15px;
  display: inline-block;
`

const Hamburger = styled.button`
  border: none;
  background: center no-repeat url(${(props) => props.isOpen ? hamburgerClose : hamburger});
  height: 30px;
  width: 30px;
  display: block;
  margin-left: auto;
  transition: opacity .5s;
  cursor: pointer;

  &:hover {
    opacity: .6;
  }

  @media screen and (min-width: ${(props) => props.theme.breakpoints.TABLET}) {
    display: none;
  }
`

class Navbar extends React.Component {
  constructor (props) {
    super(props)
    this.state = {
      value: null,
      showMobileNav: false
    }
    this.enableBody = this.enableBody.bind(this)
    this.toggleNav = this.toggleNav.bind(this)
  }

  componentDidMount () {
    window.addEventListener('resize', () => {
      this.enableBody()
      this.setState({showMobileNav: false})
    })
  }

  componentWillUnmount () {
    this.enableBody()
  }

  enableBody () {
    window.document.getElementsByTagName('body')[0].classList.remove('disabled-body')
  }

  toggleNav () {
    this.setState({showMobileNav: !this.state.showMobileNav})
    window.document.getElementsByTagName('body')[0].classList.toggle('disabled-body')
  }

  render () {
    return (
      <React.Fragment>
        <Container>
          <Inner>
            <Link to='/'>
              <Heart ml={1} mr={'24px'}>❤</Heart><Logo src={cliLogo} />
            </Link>
            <Links>
              <NavLink
                to={`cli-commands/npm${IS_STATIC ? '/index.html' : ''}`}
                partiallyActive
                activeClassName='active-navbar-link'
              >
                docs
              </NavLink>
              <BasicNavLink href='https://www.npmjs.com/'>npmjs.org</BasicNavLink>
            </Links>
            <Hamburger isOpen={this.state.showMobileNav} onClick={this.toggleNav} />
          </Inner>
        </Container>
        {this.state.showMobileNav && <MobileSidebar />}
      </React.Fragment>
    )
  }
}

export default Navbar
