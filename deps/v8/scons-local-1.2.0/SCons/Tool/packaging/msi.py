"""SCons.Tool.packaging.msi

The msi packager.
"""

#
# Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008 The SCons Foundation
# 
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

__revision__ = "src/engine/SCons/Tool/packaging/msi.py 3842 2008/12/20 22:59:52 scons"

import os
import SCons
from SCons.Action import Action
from SCons.Builder import Builder

from xml.dom.minidom import *
from xml.sax.saxutils import escape

from SCons.Tool.packaging import stripinstallbuilder

#
# Utility functions
#
def convert_to_id(s, id_set):
    """ Some parts of .wxs need an Id attribute (for example: The File and
    Directory directives. The charset is limited to A-Z, a-z, digits,
    underscores, periods. Each Id must begin with a letter or with a
    underscore. Google for "CNDL0015" for information about this.

    Requirements:
     * the string created must only contain chars from the target charset.
     * the string created must have a minimal editing distance from the
       original string.
     * the string created must be unique for the whole .wxs file.

    Observation:
     * There are 62 chars in the charset.

    Idea:
     * filter out forbidden characters. Check for a collision with the help
       of the id_set. Add the number of the number of the collision at the
       end of the created string. Furthermore care for a correct start of
       the string.
    """
    charset = 'ABCDEFGHIJKLMNOPQRSTUVWXYabcdefghijklmnopqrstuvwxyz0123456789_.'
    if s[0] in '0123456789.':
        s += '_'+s
    id = filter( lambda c : c in charset, s )

    # did we already generate an id for this file?
    try:
        return id_set[id][s]
    except KeyError:
        # no we did not so initialize with the id
        if not id_set.has_key(id): id_set[id] = { s : id }
        # there is a collision, generate an id which is unique by appending
        # the collision number
        else: id_set[id][s] = id + str(len(id_set[id]))

        return id_set[id][s]

def is_dos_short_file_name(file):
    """ examine if the given file is in the 8.3 form.
    """
    fname, ext = os.path.splitext(file)
    proper_ext = len(ext) == 0 or (2 <= len(ext) <= 4) # the ext contains the dot
    proper_fname = file.isupper() and len(fname) <= 8

    return proper_ext and proper_fname

def gen_dos_short_file_name(file, filename_set):
    """ see http://support.microsoft.com/default.aspx?scid=kb;en-us;Q142982

    These are no complete 8.3 dos short names. The ~ char is missing and 
    replaced with one character from the filename. WiX warns about such
    filenames, since a collision might occur. Google for "CNDL1014" for
    more information.
    """
    # guard this to not confuse the generation
    if is_dos_short_file_name(file):
        return file

    fname, ext = os.path.splitext(file) # ext contains the dot

    # first try if it suffices to convert to upper
    file = file.upper()
    if is_dos_short_file_name(file):
        return file

    # strip forbidden characters.
    forbidden = '."/[]:;=, '
    fname = filter( lambda c : c not in forbidden, fname )

    # check if we already generated a filename with the same number:
    # thisis1.txt, thisis2.txt etc.
    duplicate, num = not None, 1
    while duplicate:
        shortname = "%s%s" % (fname[:8-len(str(num))].upper(),\
                              str(num))
        if len(ext) >= 2:
            shortname = "%s%s" % (shortname, ext[:4].upper())

        duplicate, num = shortname in filename_set, num+1

    assert( is_dos_short_file_name(shortname) ), 'shortname is %s, longname is %s' % (shortname, file)
    filename_set.append(shortname)
    return shortname

def create_feature_dict(files):
    """ X_MSI_FEATURE and doc FileTag's can be used to collect files in a
        hierarchy. This function collects the files into this hierarchy.
    """
    dict = {}

    def add_to_dict( feature, file ):
        if not SCons.Util.is_List( feature ):
            feature = [ feature ]

        for f in feature:
            if not dict.has_key( f ):
                dict[ f ] = [ file ]
            else:
                dict[ f ].append( file )

    for file in files:
        if hasattr( file, 'PACKAGING_X_MSI_FEATURE' ):
            add_to_dict(file.PACKAGING_X_MSI_FEATURE, file)
        elif hasattr( file, 'PACKAGING_DOC' ):
            add_to_dict( 'PACKAGING_DOC', file )
        else:
            add_to_dict( 'default', file )

    return dict

def generate_guids(root):
    """ generates globally unique identifiers for parts of the xml which need
    them.

    Component tags have a special requirement. Their UUID is only allowed to
    change if the list of their contained resources has changed. This allows
    for clean removal and proper updates.

    To handle this requirement, the uuid is generated with an md5 hashing the
    whole subtree of a xml node.
    """
    from md5 import md5

    # specify which tags need a guid and in which attribute this should be stored.
    needs_id = { 'Product'   : 'Id',
                 'Package'   : 'Id',
                 'Component' : 'Guid',
               }

    # find all XMl nodes matching the key, retrieve their attribute, hash their
    # subtree, convert hash to string and add as a attribute to the xml node.
    for (key,value) in needs_id.items():
        node_list = root.getElementsByTagName(key)
        attribute = value
        for node in node_list:
            hash = md5(node.toxml()).hexdigest()
            hash_str = '%s-%s-%s-%s-%s' % ( hash[:8], hash[8:12], hash[12:16], hash[16:20], hash[20:] )
            node.attributes[attribute] = hash_str



def string_wxsfile(target, source, env):
    return "building WiX file %s"%( target[0].path )

def build_wxsfile(target, source, env):
    """ compiles a .wxs file from the keywords given in env['msi_spec'] and
        by analyzing the tree of source nodes and their tags.
    """
    file = open(target[0].abspath, 'w')

    try:
        # Create a document with the Wix root tag
        doc  = Document()
        root = doc.createElement( 'Wix' )
        root.attributes['xmlns']='http://schemas.microsoft.com/wix/2003/01/wi'
        doc.appendChild( root )

        filename_set = [] # this is to circumvent duplicates in the shortnames
        id_set       = {} # this is to circumvent duplicates in the ids

        # Create the content
        build_wxsfile_header_section(root, env)
        build_wxsfile_file_section(root, source, env['NAME'], env['VERSION'], env['VENDOR'], filename_set, id_set)
        generate_guids(root)
        build_wxsfile_features_section(root, source, env['NAME'], env['VERSION'], env['SUMMARY'], id_set)
        build_wxsfile_default_gui(root)
        build_license_file(target[0].get_dir(), env)

        # write the xml to a file
        file.write( doc.toprettyxml() )

        # call a user specified function
        if env.has_key('CHANGE_SPECFILE'):
            env['CHANGE_SPECFILE'](target, source)

    except KeyError, e:
        raise SCons.Errors.UserError( '"%s" package field for MSI is missing.' % e.args[0] )

#
# setup function
#
def create_default_directory_layout(root, NAME, VERSION, VENDOR, filename_set):
    """ Create the wix default target directory layout and return the innermost
    directory.

    We assume that the XML tree delivered in the root argument already contains
    the Product tag.

    Everything is put under the PFiles directory property defined by WiX.
    After that a directory  with the 'VENDOR' tag is placed and then a
    directory with the name of the project and its VERSION. This leads to the
    following TARGET Directory Layout:
    C:\<PFiles>\<Vendor>\<Projectname-Version>\
    Example: C:\Programme\Company\Product-1.2\
    """
    doc = Document()
    d1  = doc.createElement( 'Directory' )
    d1.attributes['Id']   = 'TARGETDIR'
    d1.attributes['Name'] = 'SourceDir'

    d2  = doc.createElement( 'Directory' )
    d2.attributes['Id']   = 'ProgramFilesFolder'
    d2.attributes['Name'] = 'PFiles'

    d3 = doc.createElement( 'Directory' )
    d3.attributes['Id']       = 'VENDOR_folder'
    d3.attributes['Name']     = escape( gen_dos_short_file_name( VENDOR, filename_set ) )
    d3.attributes['LongName'] = escape( VENDOR )

    d4 = doc.createElement( 'Directory' )
    project_folder            = "%s-%s" % ( NAME, VERSION )
    d4.attributes['Id']       = 'MY_DEFAULT_FOLDER'
    d4.attributes['Name']     = escape( gen_dos_short_file_name( project_folder, filename_set ) )
    d4.attributes['LongName'] = escape( project_folder )

    d1.childNodes.append( d2 )
    d2.childNodes.append( d3 )
    d3.childNodes.append( d4 )

    root.getElementsByTagName('Product')[0].childNodes.append( d1 )

    return d4

#
# mandatory and optional file tags
#
def build_wxsfile_file_section(root, files, NAME, VERSION, VENDOR, filename_set, id_set):
    """ builds the Component sections of the wxs file with their included files.

    Files need to be specified in 8.3 format and in the long name format, long
    filenames will be converted automatically.

    Features are specficied with the 'X_MSI_FEATURE' or 'DOC' FileTag.
    """
    root       = create_default_directory_layout( root, NAME, VERSION, VENDOR, filename_set )
    components = create_feature_dict( files )
    factory    = Document()

    def get_directory( node, dir ):
        """ returns the node under the given node representing the directory.

        Returns the component node if dir is None or empty.
        """
        if dir == '' or not dir:
            return node

        Directory = node
        dir_parts = dir.split(os.path.sep)

        # to make sure that our directory ids are unique, the parent folders are
        # consecutively added to upper_dir
        upper_dir = ''

        # walk down the xml tree finding parts of the directory
        dir_parts = filter( lambda d: d != '', dir_parts )
        for d in dir_parts[:]:
            already_created = filter( lambda c: c.nodeName == 'Directory' and c.attributes['LongName'].value == escape(d), Directory.childNodes ) 

            if already_created != []:
                Directory = already_created[0]
                dir_parts.remove(d)
                upper_dir += d
            else:
                break

        for d in dir_parts:
            nDirectory = factory.createElement( 'Directory' )
            nDirectory.attributes['LongName'] = escape( d )
            nDirectory.attributes['Name']     = escape( gen_dos_short_file_name( d, filename_set ) )
            upper_dir += d
            nDirectory.attributes['Id']       = convert_to_id( upper_dir, id_set )

            Directory.childNodes.append( nDirectory )
            Directory = nDirectory

        return Directory

    for file in files:
        drive, path = os.path.splitdrive( file.PACKAGING_INSTALL_LOCATION )
        filename = os.path.basename( path )
        dirname  = os.path.dirname( path )

        h = {
            # tagname                   : default value
            'PACKAGING_X_MSI_VITAL'     : 'yes',
            'PACKAGING_X_MSI_FILEID'    : convert_to_id(filename, id_set),
            'PACKAGING_X_MSI_LONGNAME'  : filename,
            'PACKAGING_X_MSI_SHORTNAME' : gen_dos_short_file_name(filename, filename_set),
            'PACKAGING_X_MSI_SOURCE'    : file.get_path(),
            }

        # fill in the default tags given above.
        for k,v in [ (k, v) for (k,v) in h.items() if not hasattr(file, k) ]:
            setattr( file, k, v )

        File = factory.createElement( 'File' )
        File.attributes['LongName'] = escape( file.PACKAGING_X_MSI_LONGNAME )
        File.attributes['Name']     = escape( file.PACKAGING_X_MSI_SHORTNAME )
        File.attributes['Source']   = escape( file.PACKAGING_X_MSI_SOURCE )
        File.attributes['Id']       = escape( file.PACKAGING_X_MSI_FILEID )
        File.attributes['Vital']    = escape( file.PACKAGING_X_MSI_VITAL )

        # create the <Component> Tag under which this file should appear
        Component = factory.createElement('Component')
        Component.attributes['DiskId'] = '1'
        Component.attributes['Id']     = convert_to_id( filename, id_set )

        # hang the component node under the root node and the file node
        # under the component node.
        Directory = get_directory( root, dirname )
        Directory.childNodes.append( Component )
        Component.childNodes.append( File )

#
# additional functions
#
def build_wxsfile_features_section(root, files, NAME, VERSION, SUMMARY, id_set):
    """ This function creates the <features> tag based on the supplied xml tree.

    This is achieved by finding all <component>s and adding them to a default target.

    It should be called after the tree has been built completly.  We assume
    that a MY_DEFAULT_FOLDER Property is defined in the wxs file tree.

    Furthermore a top-level with the name and VERSION of the software will be created.

    An PACKAGING_X_MSI_FEATURE can either be a string, where the feature
    DESCRIPTION will be the same as its title or a Tuple, where the first
    part will be its title and the second its DESCRIPTION.
    """
    factory = Document()
    Feature = factory.createElement('Feature')
    Feature.attributes['Id']                    = 'complete'
    Feature.attributes['ConfigurableDirectory'] = 'MY_DEFAULT_FOLDER'
    Feature.attributes['Level']                 = '1'
    Feature.attributes['Title']                 = escape( '%s %s' % (NAME, VERSION) )
    Feature.attributes['Description']           = escape( SUMMARY )
    Feature.attributes['Display']               = 'expand'

    for (feature, files) in create_feature_dict(files).items():
        SubFeature   = factory.createElement('Feature')
        SubFeature.attributes['Level'] = '1'

        if SCons.Util.is_Tuple(feature):
            SubFeature.attributes['Id']    = convert_to_id( feature[0], id_set )
            SubFeature.attributes['Title'] = escape(feature[0])
            SubFeature.attributes['Description'] = escape(feature[1])
        else:
            SubFeature.attributes['Id'] = convert_to_id( feature, id_set )
            if feature=='default':
                SubFeature.attributes['Description'] = 'Main Part'
                SubFeature.attributes['Title'] = 'Main Part'
            elif feature=='PACKAGING_DOC':
                SubFeature.attributes['Description'] = 'Documentation'
                SubFeature.attributes['Title'] = 'Documentation'
            else:
                SubFeature.attributes['Description'] = escape(feature)
                SubFeature.attributes['Title'] = escape(feature)

        # build the componentrefs. As one of the design decision is that every
        # file is also a component we walk the list of files and create a
        # reference.
        for f in files:
            ComponentRef = factory.createElement('ComponentRef')
            ComponentRef.attributes['Id'] = convert_to_id( os.path.basename(f.get_path()), id_set )
            SubFeature.childNodes.append(ComponentRef)

        Feature.childNodes.append(SubFeature)

    root.getElementsByTagName('Product')[0].childNodes.append(Feature)

def build_wxsfile_default_gui(root):
    """ this function adds a default GUI to the wxs file
    """
    factory = Document()
    Product = root.getElementsByTagName('Product')[0]

    UIRef   = factory.createElement('UIRef')
    UIRef.attributes['Id'] = 'WixUI_Mondo'
    Product.childNodes.append(UIRef)

    UIRef   = factory.createElement('UIRef')
    UIRef.attributes['Id'] = 'WixUI_ErrorProgressText'
    Product.childNodes.append(UIRef)

def build_license_file(directory, spec):
    """ creates a License.rtf file with the content of "X_MSI_LICENSE_TEXT"
    in the given directory
    """
    name, text = '', ''

    try:
        name = spec['LICENSE']
        text = spec['X_MSI_LICENSE_TEXT']
    except KeyError:
        pass # ignore this as X_MSI_LICENSE_TEXT is optional

    if name!='' or text!='':
        file = open( os.path.join(directory.get_path(), 'License.rtf'), 'w' )
        file.write('{\\rtf')
        if text!='':
             file.write(text.replace('\n', '\\par '))
        else:
             file.write(name+'\\par\\par')
        file.write('}')
        file.close()

#
# mandatory and optional package tags
#
def build_wxsfile_header_section(root, spec):
    """ Adds the xml file node which define the package meta-data.
    """
    # Create the needed DOM nodes and add them at the correct position in the tree.
    factory = Document()
    Product = factory.createElement( 'Product' )
    Package = factory.createElement( 'Package' )

    root.childNodes.append( Product )
    Product.childNodes.append( Package )

    # set "mandatory" default values
    if not spec.has_key('X_MSI_LANGUAGE'):
        spec['X_MSI_LANGUAGE'] = '1033' # select english

    # mandatory sections, will throw a KeyError if the tag is not available
    Product.attributes['Name']         = escape( spec['NAME'] )
    Product.attributes['Version']      = escape( spec['VERSION'] )
    Product.attributes['Manufacturer'] = escape( spec['VENDOR'] )
    Product.attributes['Language']     = escape( spec['X_MSI_LANGUAGE'] )
    Package.attributes['Description']  = escape( spec['SUMMARY'] )

    # now the optional tags, for which we avoid the KeyErrror exception
    if spec.has_key( 'DESCRIPTION' ):
        Package.attributes['Comments'] = escape( spec['DESCRIPTION'] )

    if spec.has_key( 'X_MSI_UPGRADE_CODE' ):
        Package.attributes['X_MSI_UPGRADE_CODE'] = escape( spec['X_MSI_UPGRADE_CODE'] )

    # We hardcode the media tag as our current model cannot handle it.
    Media = factory.createElement('Media')
    Media.attributes['Id']       = '1'
    Media.attributes['Cabinet']  = 'default.cab'
    Media.attributes['EmbedCab'] = 'yes'
    root.getElementsByTagName('Product')[0].childNodes.append(Media)

# this builder is the entry-point for .wxs file compiler.
wxs_builder = Builder(
    action         = Action( build_wxsfile, string_wxsfile ),
    ensure_suffix  = '.wxs' )

def package(env, target, source, PACKAGEROOT, NAME, VERSION,
            DESCRIPTION, SUMMARY, VENDOR, X_MSI_LANGUAGE, **kw):
    # make sure that the Wix Builder is in the environment
    SCons.Tool.Tool('wix').generate(env)

    # get put the keywords for the specfile compiler. These are the arguments
    # given to the package function and all optional ones stored in kw, minus
    # the the source, target and env one.
    loc = locals()
    del loc['kw']
    kw.update(loc)
    del kw['source'], kw['target'], kw['env']

    # strip the install builder from the source files
    target, source = stripinstallbuilder(target, source, env)

    # put the arguments into the env and call the specfile builder.
    env['msi_spec'] = kw
    specfile = apply( wxs_builder, [env, target, source], kw )

    # now call the WiX Tool with the built specfile added as a source.
    msifile  = env.WiX(target, specfile)

    # return the target and source tuple.
    return (msifile, source+[specfile])

