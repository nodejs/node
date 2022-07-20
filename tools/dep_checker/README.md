# Node.js dependency vulnerability checker

This script queries the [National Vulnerability Database (NVD)](https://nvd.nist.gov/) and
the [GitHub Advisory Database](https://github.com/advisories) for vulnerabilities found
in Node's dependencies.

## How to use

### Database authentication

- In order to query the GitHub Advisory Database,
  a [Personal Access Token](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/creating-a-personal-access-token)
  has to be created (no permissions need to be given to the token, since it's only used to query the public database).
- The NVD can be queried without authentication, but it will be rate limited to one query every six seconds. In order to
  remove
  that limitation [request an API key](https://nvd.nist.gov/developers/request-an-api-key) and pass it as a parameter.

### Running the script

Once acquired, the script can be run as follows:

```shell
cd node/tools/dep_checker/
pip install -r requirements.txt

# Python >= 3.9 required
python main.py --gh-token=$PERSONAL_ACCESS_TOKEN --nvd-key=$NVD_API_KEY

# The command can also be run without parameters
# This will skip querying the GitHub Advisory Database, and query the NVD
# using the anonymous (rate-limited) API
python main.py
```

## Example output

```
WARNING: New vulnerabilities found
- npm (version 1.2.1) :
	- GHSA-v3jv-wrf4-5845: https://github.com/advisories/GHSA-v3jv-wrf4-5845
	- GHSA-93f3-23rq-pjfp: https://github.com/advisories/GHSA-93f3-23rq-pjfp
	- GHSA-m6cx-g6qm-p2cx: https://github.com/advisories/GHSA-m6cx-g6qm-p2cx
	- GHSA-4328-8hgf-7wjr: https://github.com/advisories/GHSA-4328-8hgf-7wjr
	- GHSA-x8qc-rrcw-4r46: https://github.com/advisories/GHSA-x8qc-rrcw-4r46
	- GHSA-m5h6-hr3q-22h5: https://github.com/advisories/GHSA-m5h6-hr3q-22h5
- acorn (version 6.0.0) :
	- GHSA-6chw-6frg-f759: https://github.com/advisories/GHSA-6chw-6frg-f759

For each dependency and vulnerability, check the following:
- Check the vulnerability's description to see if it applies to the dependency as
used by Node. If not, the vulnerability ID (either a CVE or a GHSA) can be added to the ignore list in
dependencies.py. IMPORTANT: Only do this if certain that the vulnerability found is a false positive.
- Otherwise, the vulnerability found must be remediated by updating the dependency in the Node repo to a
non-affected version.
```

## Implementation details

- For each dependency in Node's `deps/` folder, the script parses their version number and queries the databases to find
  vulnerabilities for that specific version.
- The queries can return false positives (
  see [this](https://github.com/nodejs/security-wg/issues/802#issuecomment-1144207417) comment for an example). These
  can be ignored by adding the vulnerability to the `ignore_list` in `dependencies.py`
- If no NVD API key is provided, the script will take a while to finish (~2 min) because queries to the NVD
  are [rate-limited](https://nvd.nist.gov/developers/start-here)
- If any vulnerabilities are found, the script returns 1 and prints out a list with the ID and a link to a description
  of
  the vulnerability. This is the case except when the ID matches one in the ignore-list (inside `dependencies.py`) in
  which case the vulnerability is ignored.



