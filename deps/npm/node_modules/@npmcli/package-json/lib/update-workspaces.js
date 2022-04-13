const updateWorkspaces = ({ content, originalContent = {} }) => {
  const newWorkspaces = content.workspaces

  if (!newWorkspaces)
    return originalContent

  // validate workspaces content being appended
  const hasInvalidWorkspaces = () =>
    newWorkspaces.some(w => !(typeof w === 'string'))
  if (!newWorkspaces.length || hasInvalidWorkspaces()) {
    throw Object.assign(
      new TypeError('workspaces should be an array of strings.'),
      { code: 'EWORKSPACESINVALID' }
    )
  }

  return {
    ...originalContent,
    workspaces: [
      ...newWorkspaces,
    ],
  }
}

module.exports = updateWorkspaces
