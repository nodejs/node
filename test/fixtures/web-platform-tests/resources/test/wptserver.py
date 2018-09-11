import logging
import os
import subprocess
import time
import sys
import urllib2


class WPTServer(object):
    def __init__(self, wpt_root):
        self.wpt_root = wpt_root

        # This is a terrible hack to get the default config of wptserve.
        sys.path.insert(0, os.path.join(wpt_root, "tools"))
        from serve.serve import build_config
        with build_config() as config:
            self.host = config["browser_host"]
            self.http_port = config["ports"]["http"][0]
            self.https_port = config["ports"]["https"][0]

        self.base_url = 'http://%s:%s' % (self.host, self.http_port)
        self.https_base_url = 'https://%s:%s' % (self.host, self.https_port)

    def start(self):
        self.devnull = open(os.devnull, 'w')
        wptserve_cmd = [os.path.join(self.wpt_root, 'wpt'), 'serve']
        logging.info('Executing %s' % ' '.join(wptserve_cmd))
        self.proc = subprocess.Popen(
            wptserve_cmd,
            stderr=self.devnull,
            cwd=self.wpt_root)

        for retry in range(5):
            # Exponential backoff.
            time.sleep(2 ** retry)
            exit_code = self.proc.poll()
            if exit_code != None:
                logging.warn('Command "%s" exited with %s', ' '.join(wptserve_cmd), exit_code)
                break
            try:
                urllib2.urlopen(self.base_url, timeout=1)
                return
            except urllib2.URLError:
                pass

        raise Exception('Could not start wptserve on %s' % self.base_url)

    def stop(self):
        self.proc.terminate()
        self.proc.wait()
        self.devnull.close()

    def url(self, abs_path):
        return self.https_base_url + '/' + os.path.relpath(abs_path, self.wpt_root)
