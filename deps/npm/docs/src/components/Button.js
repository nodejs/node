import {Link} from 'gatsby'
import {colors} from '../theme'
import styled from 'styled-components'

export const LinkButton = styled(Link)`
  background-color: ${colors.red};
  color: ${colors.white};
  font-size: 20px;
  border-radius: 1px;
  padding: 20px;
  box-shadow: 8px 8px 0 rgba(251,59,73,.2);
  text-decoration: none;
  text-align: center;
  display: inline-block;
  min-width: 180px;
  font-weight: 700;
  transition: opacity .5s;

  &:hover {
    opacity: .8;
  }
`
