Description
------------------------------
This directory is used to build released wheels according to PEP513 and upload
them to pypi.

Usage
------------------------------
For example, to release 3.3.0:
    ./protobuf_optimized_pip.sh 3.3.0 PYPI_USERNAME PYPI_PASSWORD

Structure
------------------------------
| Source                    | Source                                                       |
|--------------------------------------|---------------------------------------------------|
| protobuf_optimized_pip.sh | Entry point. Calling Dockerfile and build_wheel_manylinux.sh |
| Dockerfile                | Build docker image according to PEP513.                      |
| build_wheel_manylinux.sh  | Build wheel packages in the docker container.                |
