""" Node.js dependency vulnerability checker

This script queries the National Vulnerability Database (NVD) and the GitHub Advisory Database for vulnerabilities found
in Node's dependencies.

For each dependency in Node's `deps/` folder, the script parses their version number and queries the databases to find
vulnerabilities for that specific version.

If any vulnerabilities are found, the script returns 1 and prints out a list with the ID and a link to a description of
the vulnerability. This is the case except when the ID matches one in the ignore-list (inside `dependencies.py`) in
which case the vulnerability is ignored.
"""

from argparse import ArgumentParser
from collections import defaultdict
from dependencies import ignore_list, dependencies
from gql import gql, Client
from gql.transport.aiohttp import AIOHTTPTransport
from nvdlib import searchCVE  # type: ignore
from packaging.specifiers import SpecifierSet
from typing import Optional


class Vulnerability:
    def __init__(self, id: str, url: str):
        self.id = id
        self.url = url


vulnerability_found_message = """For each dependency and vulnerability, check the following:
- Check that the dependency's version printed by the script corresponds to the version present in the Node repo.
If not, update dependencies.py with the actual version number and run the script again.
- If the version is correct, check the vulnerability's description to see if it applies to the dependency as
used by Node. If not, the vulnerability ID (either a CVE or a GHSA) can be added to the ignore list in
dependencies.py. IMPORTANT: Only do this if certain that the vulnerability found is a false positive.
- Otherwise, the vulnerability found must be remediated by updating the dependency in the Node repo to a
non-affected version, followed by updating dependencies.py with the new version.
"""


github_vulnerabilities_query = gql(
    """
    query($package_name:String!) {
      securityVulnerabilities(package:$package_name, last:10) {
        nodes {
          vulnerableVersionRange
          advisory {
            ghsaId
            permalink
            withdrawnAt
          }
        }
      }
    }
"""
)


def query_ghad(gh_token: str) -> dict[str, list[Vulnerability]]:
    """Queries the GitHub Advisory Database for vulnerabilities reported for Node's dependencies.

    The database supports querying by package name in the NPM ecosystem, so we only send queries for the dependencies
    that are also NPM packages.
    """

    deps_in_npm = {
        name: dep for name, dep in dependencies.items() if dep.npm_name is not None
    }

    transport = AIOHTTPTransport(
        url="https://api.github.com/graphql",
        headers={"Authorization": f"bearer {gh_token}"},
    )
    client = Client(
        transport=transport,
        fetch_schema_from_transport=True,
        serialize_variables=True,
        parse_results=True,
    )

    found_vulnerabilities: dict[str, list[Vulnerability]] = defaultdict(list)
    for name, dep in deps_in_npm.items():
        variables_package = {
            "package_name": dep.npm_name,
        }
        result = client.execute(
            github_vulnerabilities_query, variable_values=variables_package
        )
        matching_vulns = [
            v
            for v in result["securityVulnerabilities"]["nodes"]
            if v["advisory"]["withdrawnAt"] is None
            and dep.version in SpecifierSet(v["vulnerableVersionRange"])
            and v["advisory"]["ghsaId"] not in ignore_list
        ]
        if matching_vulns:
            found_vulnerabilities[name].extend(
                [
                    Vulnerability(
                        id=vuln["advisory"]["ghsaId"], url=vuln["advisory"]["permalink"]
                    )
                    for vuln in matching_vulns
                ]
            )

    return found_vulnerabilities


def query_nvd(api_key: Optional[str]) -> dict[str, list[Vulnerability]]:
    """Queries the National Vulnerability Database for vulnerabilities reported for Node's dependencies.

    The database supports querying by CPE (Common Platform Enumeration) or by a keyword present in the CVE's
    description.
    Since some of Node's dependencies don't have an associated CPE, we use their name as a keyword in the query.
    """
    deps_in_nvd = {
        name: dep
        for name, dep in dependencies.items()
        if dep.cpe is not None or dep.keyword is not None
    }
    found_vulnerabilities: dict[str, list[Vulnerability]] = defaultdict(list)
    for name, dep in deps_in_nvd.items():
        query_results = [
            cve
            for cve in searchCVE(
                cpeMatchString=dep.get_cpe(), keyword=dep.keyword, key=api_key
            )
            if cve.id not in ignore_list
        ]
        if query_results:
            found_vulnerabilities[name].extend(
                [Vulnerability(id=cve.id, url=cve.url) for cve in query_results]
            )

    return found_vulnerabilities


def main():
    parser = ArgumentParser(
        description="Query the NVD and the GitHub Advisory Database for new vulnerabilities in Node's dependencies"
    )
    parser.add_argument(
        "--gh-token",
        help="the GitHub authentication token for querying the GH Advisory Database",
    )
    parser.add_argument(
        "--nvd-key",
        help="the NVD API key for querying the National Vulnerability Database",
    )
    gh_token = parser.parse_args().gh_token
    nvd_key = parser.parse_args().nvd_key
    if gh_token is None:
        print(
            "Warning: GitHub authentication token not provided, skipping GitHub Advisory Database queries"
        )
    if nvd_key is None:
        print(
            "Warning: NVD API key not provided, queries will be slower due to rate limiting"
        )
    ghad_vulnerabilities: dict[str, list[Vulnerability]] = (
        {} if gh_token is None else query_ghad(gh_token)
    )
    nvd_vulnerabilities: dict[str, list[Vulnerability]] = query_nvd(nvd_key)

    if not ghad_vulnerabilities and not nvd_vulnerabilities:
        print(f"No new vulnerabilities found ({len(ignore_list)} ignored)")
        return 0
    else:
        print("WARNING: New vulnerabilities found")
        for source in (ghad_vulnerabilities, nvd_vulnerabilities):
            for name, vulns in source.items():
                print(f"- {name} (version {dependencies[name].version}) :")
                for v in vulns:
                    print(f"\t- {v.id}: {v.url}")
        print(f"\n{vulnerability_found_message}")
        return 1


if __name__ == "__main__":
    exit(main())
