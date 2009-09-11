<!--
  Inlcuded in xhtml.xsl, xhtml.chunked.xsl, htmlhelp.xsl.
  Contains common XSL stylesheets parameters.
  Output documents styled by docbook.css.
-->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:param name="html.stylesheet" select="'./docbook-xsl.css'"/>

<xsl:param name="htmlhelp.chm" select="'htmlhelp.chm'"/>
<xsl:param name="htmlhelp.hhc.section.depth" select="5"/>

<xsl:param name="suppress.navigation" select="0"/>
<xsl:param name="navig.graphics.extension" select="'.png'"/>
<xsl:param name="navig.graphics" select="0"/>
<xsl:param name="navig.graphics.path">./images/icons/</xsl:param>
<xsl:param name="navig.showtitles">0</xsl:param>

<xsl:param name="shade.verbatim" select="0"/>
<xsl:attribute-set name="shade.verbatim.style">
  <xsl:attribute name="border">0</xsl:attribute>
  <xsl:attribute name="bgcolor">#E0E0E0</xsl:attribute>
</xsl:attribute-set>

<xsl:param name="admon.graphics" select="1"/>
<xsl:param name="admon.graphics.path">./images/icons/</xsl:param>
<xsl:param name="admon.graphics.extension" select="'.png'"/>
<xsl:param name="admon.style">
  <xsl:text>margin-left: 0; margin-right: 10%;</xsl:text>
</xsl:param>
<xsl:param name="admon.textlabel" select="1"/>

<xsl:param name="callout.defaultcolumn" select="'60'"/>
<xsl:param name="callout.graphics.extension" select="'.png'"/>
<xsl:param name="callout.graphics" select="'1'"/>
<xsl:param name="callout.graphics.number.limit" select="'10'"/>
<xsl:param name="callout.graphics.path" select="'./images/icons/callouts/'"/>
<xsl:param name="callout.list.table" select="'1'"/>

<xsl:param name="base.dir" select="'./xhtml/'"/>
<xsl:param name="chunk.first.sections" select="0"/>
<xsl:param name="chunk.quietly" select="0"/>
<xsl:param name="chunk.section.depth" select="1"/>
<xsl:param name="chunk.toc" select="''"/>
<xsl:param name="chunk.tocs.and.lots" select="0"/>

<xsl:param name="html.cellpadding" select="'4px'"/>
<xsl:param name="html.cellspacing" select="''"/>

<xsl:param name="table.borders.with.css" select="1"/>
<xsl:param name="table.cell.border.color" select="''"/>

<xsl:param name="table.cell.border.style" select="'solid'"/>
<xsl:param name="table.cell.border.thickness" select="'1px'"/>
<xsl:param name="table.footnote.number.format" select="'a'"/>
<xsl:param name="table.footnote.number.symbols" select="''"/>
<xsl:param name="table.frame.border.color" select="'#527bbd'"/>
<xsl:param name="table.frame.border.style" select="'solid'"/>
<xsl:param name="table.frame.border.thickness" select="'2px'"/>
<xsl:param name="tablecolumns.extension" select="'1'"/>

</xsl:stylesheet>
