#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from tll import asynctll
import tll.channel as C
from tll.error import TLLError
from tll.test_util import ports

import decorator
import enum
import http.server
import os
import pytest
import socket
import socketserver

@pytest.fixture
def context():
    ctx = C.Context()
    try:
        ctx.load(os.path.join(os.environ.get("BUILD_DIR", "build"), "tll-curl"))
    except:
        pytest.skip("curl:// channel not available")
    return ctx

@pytest.fixture
def asyncloop(context):
    loop = asynctll.Loop(context)
    yield loop
    loop.destroy()
    loop = None

class EchoHandler(http.server.BaseHTTPRequestHandler):
    def version_string(self): return 'EchoServer/1.0'
    def date_time_string(self, timestamp=None): return 'today'

    def _reply(self, code, body, headers={}):
        self.send_response(code)
        if 'Content-Type' not in headers:
            self.send_header("Content-Type", "text/plain")
        for k,v in sorted(headers.items()):
            self.send_header(k, v)
        self.end_headers()

        self.wfile.write(body)

    def do_GET(self): return self._reply(200, f'GET {self.path}'.encode('ascii'))
    def do_POST(self):
        for k,v in self.headers.items():
            print(f'{k}: {v}')
        size = int(self.headers.get('Content-Length', '-1'))
        if size == -1:
            size = 0
        self.rfile.raw._sock.setblocking(False)
        data = f'POST {self.path} :'.encode('ascii') + (self.rfile.read(size) or b'')

        headers = {'Content-Length':str(len(data))}
        if 'X-Test-Header' in self.headers:
            headers['X-Test-Header'] = self.headers['X-Test-Header']

        return self._reply(500, data, headers)

HEADERS = [
    {'header': 'content-type', 'value': 'text/plain'},
    {'header': 'date', 'value': 'today'},
    {'header': 'server', 'value': 'EchoServer/1.0'}
]

class HTTPServer(socketserver.TCPServer):
    timeout = 0.1
    address_family = socket.AF_INET6
    allow_reuse_address = True

    def __init__(self, *a, **kw):
        super().__init__(*a, **kw)

@pytest.fixture
def port():
    return ports.TCP6

@pytest.fixture
def httpd(port):
    with HTTPServer(('::1', port), EchoHandler) as httpd:
        yield httpd

@decorator.decorator
def asyncloop_run(f, asyncloop, *a, **kw):
    asyncloop.run(f(asyncloop, *a, **kw))

class Method(enum.Enum):
    UNDEFINED = -1

@asyncloop_run
async def test_autoclose(asyncloop, port, httpd):
    c = asyncloop.Channel('curl+http://[::1]:{}/some/path'.format(port), autoclose='yes', dump='text', name='http')
    c.open()

    await asyncloop.sleep(0.01)

    httpd.handle_request()

    m = await c.recv()
    assert m.type == m.Type.Control
    assert c.unpack(m).as_dict() == {'code': 200, 'method': Method.UNDEFINED, 'headers': HEADERS, 'path': f'http://[::1]:{port}/some/path', 'size': -1}

    m = await c.recv()
    assert m.data.tobytes() == b'GET /some/path'

    await asyncloop.sleep(0.001)
    assert c.state == c.State.Closed

@asyncloop_run
async def test_autoclose_many(asyncloop, port, httpd):
    multi = asyncloop.Channel('curl://', name='multi')
    multi.open()

    c0 = asyncloop.Channel('curl+http://[::1]:{}/c0'.format(port), autoclose='yes', dump='text', name='c0', master=multi)
    c0.open()

    c1 = asyncloop.Channel('curl+http://[::1]:{}/c1'.format(port), autoclose='yes', dump='text', name='c1', master=multi, method='POST')
    c1.open()

    await asyncloop.sleep(0.01)

    httpd.handle_request()

    m = await c0.recv()
    assert m.type == m.Type.Control
    assert c0.unpack(m).as_dict() == {'code': 200, 'method': Method.UNDEFINED, 'headers': HEADERS, 'path': f'http://[::1]:{port}/c0', 'size': -1}

    m = await c0.recv(0.11)
    assert m.data.tobytes() == b'GET /c0'

    httpd.handle_request()

    m = await c1.recv()
    assert m.type == m.Type.Control
    assert c1.unpack(m).as_dict() == {
        'code': 500,
        'method': Method.UNDEFINED,
        'size': 10,
        'headers': [{'header': 'content-length', 'value': '10'}] + HEADERS,
        'path': f'http://[::1]:{port}/c1',
    }

    m = await c1.recv(0.12)
    assert m.data.tobytes() == b'POST /c1 :'

    await asyncloop.sleep(0.001)
    assert c0.state == c0.State.Closed
    assert c1.state == c1.State.Closed

@asyncloop_run
async def test_data(asyncloop, port, httpd):
    c = asyncloop.Channel('curl+http://[::1]:{}/post'.format(port), dump='text', name='post', transfer='data', method='POST', **{'expect-timeout': '1000ms', 'header.Expect':'', 'header.X-Test-Header': 'value'})
    c.open()

    await asyncloop.sleep(0.01)

    httpd.handle_request()

    with pytest.raises(TimeoutError): await c.recv(0.01)

    for data in [b'xxx', b'zzzz']:
        c.post(data)

        await asyncloop.sleep(0.01)

        httpd.handle_request()

        m = await c.recv(0.01)
        assert m.type == m.Type.Control
        assert m.addr == 0
        assert c.unpack(m).as_dict() == {
            'code': 500,
            'method': Method.UNDEFINED,
            'size': 12 + len(data),
            'headers': [{'header': 'content-length', 'value': str(12 + len(data))}] + HEADERS + [{'header': 'x-test-header', 'value': 'value'}],
            'path': f'http://[::1]:{port}/post',
        }

        m = await c.recv(0.02)
        assert m.addr == 0
        assert m.data.tobytes() == b'POST /post :' + data

        m = await c.recv(0.01)
        assert m.type == m.Type.Control
        assert m.addr == 0
        assert c.unpack(m).as_dict() == {
            'code': 0,
            'error': ''
        }

        await asyncloop.sleep(0.001)

        assert c.state == c.State.Active

    for addr, data in enumerate([b'xxx', b'zzzz']):
        c.post(data, addr=addr)

    for addr, data in enumerate([b'xxx', b'zzzz']):
        await asyncloop.sleep(0.01)

        httpd.handle_request()

        m = await c.recv(0.01)
        assert m.type == m.Type.Control
        assert m.addr == addr
        assert c.unpack(m).as_dict() == {
            'code': 500,
            'method': Method.UNDEFINED,
            'size': 12 + len(data),
            'headers': [{'header': 'content-length', 'value': str(12 + len(data))}] + HEADERS + [{'header': 'x-test-header', 'value': 'value'}],
            'path': f'http://[::1]:{port}/post',
        }

        m = await c.recv(0.02)
        assert m.addr == addr
        assert m.data.tobytes() == b'POST /post :' + data

        m = await c.recv(0.01)
        assert m.type == m.Type.Control
        assert m.addr == addr
        assert c.unpack(m).as_dict() == {
            'code': 0,
            'error': ''
        }

        await asyncloop.sleep(0.001)

        assert c.state == c.State.Active

@asyncloop_run
async def test_disconnect(asyncloop, port, httpd):
    c = asyncloop.Channel('curl+http://localhost:9/', autoclose='yes', dump='text', name='http')
    c.open()

    m = await c.recv(0.5)
    assert m.type == m.Type.Control
    assert m.msgid == c.scheme_control['Disconnect'].msgid
    m = c.unpack(m)
    assert m.code == 7
    #assert m.error == "Couldn't connect to server" # Error message is not fixed

@asyncloop_run
async def test_post_disconnect(asyncloop, port, httpd):
    c = asyncloop.Channel(f'curl+http://[::1]:{port}/post', dump='text', name='http', transfer='data', method='POST')
    c.open()

    DISCONNECT = c.scheme_control['Disconnect'].msgid
    CONNECT = c.scheme_control['Connect'].msgid

    c.post(b'xxx', addr=0)
    c.post(b'zzz', addr=1)
    c.post(b'', type=c.Type.Control, msgid=DISCONNECT, addr=0)

    await asyncloop.sleep(0.01)

    httpd.handle_request()
    httpd.handle_request()

    m = await c.recv(0.01)
    assert m.type == m.Type.Control
    assert m.msgid == CONNECT
    assert m.addr == 1

@asyncloop_run
async def test_control(asyncloop, port, httpd):
    c = asyncloop.Channel('curl+http://[::1]:{}/post'.format(port), dump='text', name='post', transfer='control', method='POST', **{'expect-timeout': '1000ms', 'header.Expect':''})
    c.open()

    await asyncloop.sleep(0.01)

    httpd.handle_request()

    with pytest.raises(TimeoutError): await c.recv(0.01)

    data = [b'xxx', b'zzzzzzz']

    for i,d in enumerate(data):
        msg = {'path': f'/extra/{i}' , 'size': len(d), 'headers': [{'header': 'X-Test-Header', 'value': str(i)}]}
        c.post(msg, name='Connect', type=c.Type.Control, addr=i)

    for i,d in enumerate(data):
        c.post(d, addr=i)

    for i,d in enumerate(data):
        await asyncloop.sleep(0.01)

        httpd.handle_request()

        m = await c.recv(0.01)
        assert m.type == m.Type.Control
        assert m.addr == i
        assert c.unpack(m).as_dict() == {
            'code': 500,
            'method': Method.UNDEFINED,
            'size': 20 + len(d),
            'headers': [{'header': 'content-length', 'value': str(20 + len(d))}] + HEADERS + [{'header': 'x-test-header', 'value': str(i)}],
            'path': f'http://[::1]:{port}/post/extra/{i}',
        }

        m = await c.recv(0.02)
        assert m.addr == i
        assert m.data.tobytes() == b'POST /post/extra/%d :' % i + d

        m = await c.recv(0.01)
        assert m.type == m.Type.Control
        assert m.addr == i
        assert c.unpack(m).as_dict() == {
            'code': 0,
            'error': ''
        }

        await asyncloop.sleep(0.001)

        assert c.state == c.State.Active
