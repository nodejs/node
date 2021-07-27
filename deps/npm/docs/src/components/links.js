import {Link} from 'gatsby'
import styled, {css} from 'styled-components'

const baseLinkStyles = css`
  font-weight: 500;
  text-decoration: none;
  letter-spacing: .3px;
  font-size: 14px;
`
const featureLinkStyles = css`
  ${baseLinkStyles}
  color: ${(props) => props.theme.colors.black};
  transition: opacity .5s
  &:hover {
    opacity: .9;
  }
`

const navLinkStyles = css`
  ${baseLinkStyles};
  color: ${(props) => props.theme.colors.black};
  transition: opacity .5s;
  margin: 0 10px;

  &:hover {
    opacity: .5;
  }
`
export const FeatureLink = styled(Link)`
  ${featureLinkStyles}
`

export const NavLink = styled(Link)`
  ${navLinkStyles};
`

export const BasicNavLink = styled.a`
  ${navLinkStyles};
`

export const SidebarLink = styled(Link)`
  ${baseLinkStyles};
  color: ${(props) => props.theme.colors.red};
  padding: 10px;
  transition: background-color .3s;

  &:hover {
    background-color: ${(props) => props.theme.colors.lightPurple};
  }
`
