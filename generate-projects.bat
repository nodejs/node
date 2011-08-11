@echo off
python tools\gyp_node -f msvs -G msvs_version=2010 -D msvs_multi_core_compile=true
