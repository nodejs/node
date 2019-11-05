import React from 'react'
import styled from 'styled-components'
import {StaticQuery, graphql} from 'gatsby'
import {Flex} from 'rebass'
import {SidebarLink} from './links'
import Accordion from './Accordion'

const IS_STATIC = process.env.GATSBY_IS_STATIC

const LinkDesc = styled.span`
  font-size: 11px; 
  line-height: 1.5; 
  text-transform: lowercase;
  display: block;
  font-weight: 400;
  color: ${(props) => props.theme.colors.darkGray};
`

const DocLinks = ({data}) => {
  const linkInfo = data.allMarkdownRemark.nodes
  const sections = ['cli-commands', 'configuring-npm', 'using-npm']
  let sortedData = {}

  sections.map((section) => (
    sortedData[section] = linkInfo.filter(function (item) {
      return item.frontmatter.section === section
    })
  ))

  return sections.map((section, index) => (
    <Accordion key={index} section={section}>
      {sortedData[section].map((linkData, index) => {
        const title = section === 'cli-commands'
          ? linkData.frontmatter.title.replace(/(npm-)+([a-zA-Z\\.-]*)/, 'npm $2')
          : linkData.frontmatter.title

        return (
          <Flex flexDirection='column' key={index}>
            <SidebarLink
              to={`${linkData.fields.slug}${IS_STATIC ? '/index.html' : ''}`}
              activeClassName='active-sidebar-link'
            >
              {title}
              <LinkDesc>{linkData.frontmatter.description}</LinkDesc>
            </SidebarLink>
          </Flex>
        )
      })
      }
    </Accordion>
  ))
}

export default props => (
  <StaticQuery
    query={graphql`
      query sortedLinkData {
        allMarkdownRemark(sort: {fields: frontmatter___title}) {
          nodes {
            fields {
              slug
            }
            frontmatter {
              description
              section
              title
            }
          }
        }
      }
    `}
    render={data => <DocLinks data={data} {...props} />}
  />
)
