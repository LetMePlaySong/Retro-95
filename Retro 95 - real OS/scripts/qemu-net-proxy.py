#!/usr/bin/env python3
import os
import re
import socket
import sys
import threading
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

LISTEN_HOST = os.environ.get('QEMU_NET_HOST', '0.0.0.0')
LISTEN_PORT = int(os.environ.get('QEMU_NET_PORT', '8080'))
DEFAULT_HOST = os.environ.get('QEMU_NET_DEFAULT_HOST', 'example.com')


def build_url(host, path):
    host = host.strip()
    if not host:
        host = DEFAULT_HOST
    if re.match(r'^https?://', path, re.I):
        return path
    if not re.match(r'^https?://', host, re.I):
        scheme = 'https://'
    else:
        scheme = ''
    if path.startswith('/'):
        return f"{scheme}{host}{path}"
    return f"{scheme}{host}/{path}"


class ProxyHandler(BaseHTTPRequestHandler):
    protocol_version = 'HTTP/1.1'
    server_version = 'Retro95NetProxy/1.0'

    def do_GET(self):
        target = self.path
        host = self.headers.get('Host', DEFAULT_HOST)
        url = build_url(host, target)
        try:
            req = urllib.request.Request(url, headers={
                'User-Agent': 'Retro95/1.0 (+QEMU e1000 proxy)',
                'Accept': '*/*',
                'Accept-Language': 'en-US,en;q=0.9',
            })
            with urllib.request.urlopen(req, timeout=15) as resp:
                payload = resp.read()
                mime = resp.headers.get_content_type() or 'application/octet-stream'
                self.send_response(resp.status)
                self.send_header('Content-Type', mime)
                self.send_header('Content-Length', str(len(payload)))
                self.send_header('Access-Control-Allow-Origin', '*')
                self.send_header('Cache-Control', 'no-store')
                self.end_headers()
                if payload:
                    self.wfile.write(payload)
        except Exception as exc:
            body = (
                b"<html><body><h1>Retro 95 network proxy</h1>"
                b"<p>Fetch failed: " + str(exc).encode() + b"</p></body></html>"
            )
            self.send_response(502)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    def log_message(self, format, *args):
        return


if __name__ == '__main__':
    try:
        server = ThreadingHTTPServer((LISTEN_HOST, LISTEN_PORT), ProxyHandler)
        print(f"Retro 95 proxy listening on {LISTEN_HOST}:{LISTEN_PORT}", flush=True)
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    except OSError as exc:
        print(f"Proxy startup failed: {exc}", file=sys.stderr)
        sys.exit(1)
