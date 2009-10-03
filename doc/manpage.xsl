<!--
  Generates single roff manpage document from DocBook XML source using DocBook
  XSL stylesheets.

  NOTE: The URL reference to the current DocBook XSL stylesheets is
  rewritten to point to the copy on the local disk drive by the XML catalog
  rewrite directives so it doesn't need to go out to the Internet for the
  stylesheets. This means you don't need to edit the <xsl:import> elements on
  a machine by machine basis.
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"/>
<xsl:import href="common.xsl"/>

<!-- Only render the link text -->
<xsl:template match="ulink">
  <xsl:variable name="content">
    <xsl:apply-templates/>
  </xsl:variable>
  <xsl:value-of select="$content"/>
</xsl:template>

<!-- Don't automatically generate the REFERENCES section -->
<xsl:template name="format.links.list">
</xsl:template>

</xsl:stylesheet>

