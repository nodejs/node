# Offboarding

This document is a checklist of things to do when a collaborator becomes
emeritus or leaves the project.

* Remove the collaborator from the @nodejs/collaborators team.
* Open a fast-track pull request to move the collaborator to the collaborator
  emeriti list in README.md.
* Determine what GitHub teams the collaborator belongs to. In consultation with
  the collaborator, determine which of those teams they should be removed from.
  * Some teams may also require a pull request to remove the collaborator from
    a team listing. For example, if someone is removed from @nodejs/build,
    they should also be removed from the Build WG README.md file in the
    <https://github.com/nodejs/build> repository.
  * When in doubt, especially if you are unable to get in contact with the
    collaborator, remove them from all teams. It is easy enough to add them
    back later, so we err on the side of privacy and security.
* Open an issue in the [build](https://github.com/nodejs/build) repository
  titled `Remove Collaborator from Coverity` asking that the collaborator
  be removed from the Node.js coverity project if they had access.
