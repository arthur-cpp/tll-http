#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import decorator
import os
import pytest

from tll import asynctll
from tll.channel import Context
from tll.error import TLLError
from tll.test_util import ports

@pytest.fixture
def context():
    ctx = Context()
    try:
        ctx.load(os.path.join(os.environ.get("BUILD_DIR", "build"), "tll-uws"))
        ctx.load(os.path.join(os.environ.get("BUILD_DIR", "build"), "tll-curl"))
    except:
        pytest.skip("uws:// or curl:// channels not available")
    return ctx

@pytest.fixture
def server(asyncloop, port):
    c = asyncloop.Channel(f'uws://*:{port}', name='server')
    yield c
    c.close()

@pytest.fixture
def asyncloop(context):
    loop = asynctll.Loop(context)
    yield loop
    loop.destroy()
    loop = None

@pytest.fixture
def port():
    return ports.TCP6

@decorator.decorator
def asyncloop_run(f, asyncloop, *a, **kw):
    asyncloop.run(f(asyncloop, *a, **kw))

@pytest.fixture
def client(asyncloop, port):
    c = asyncloop.Channel(f'curl+http://127.0.0.1:{port}', transfer='control', name='client', dump='frame')
    yield c
    c.close()
    del c

async def check_response(c, addr, connect={}, data=None):
    m = await c.recv()
    assert (m.type, m.addr) == (m.Type.Control, addr)
    m = c.unpack(m)
    assert m.SCHEME.name == 'Connect'
    assert {k:v for k,v in m.as_dict().items() if k in connect} == connect

    m = await c.recv()
    assert (m.type, m.addr) == (m.Type.Data, addr)
    if data is not None:
        assert m.data == data

    m = await c.recv()
    assert (m.type, m.addr) == (m.Type.Control, addr)
    assert c.unpack(m).SCHEME.name == 'Disconnect'

@asyncloop_run
async def test_http(asyncloop, server, client):
    sub = asyncloop.Channel("uws+http://path", master=server, name='server/http', dump='yes');

    server.open()
    client.open()

    client.post({'path':'/path'}, type=client.Type.Control, name='Connect', addr=1)
    await check_response(client, 1, {'code':404})

    sub.open()

    client.post({'path':'/abc'}, type=client.Type.Control, name='Connect', addr=2)
    await check_response(client, 2, {'code':404})

    client.post({'path':'/path'}, type=client.Type.Control, name='Connect', addr=3)

    m = await sub.recv()

    assert m.type == m.Type.Control

    m = await sub.recv()

    assert m.type == m.Type.Data
    assert m.data.tobytes() == b''

    sub.post(b'hello', addr=m.addr)

    await check_response(client, 3, {'code':200}, b'hello')

@asyncloop_run
async def test_http_data(asyncloop, server, port):
    client = asyncloop.Channel(f'curl+http://127.0.0.1:{port}/path', transfer='data', name='client', dump='frame')

    sub = asyncloop.Channel("uws+http://path", master=server, name='server/http', dump='yes');

    server.open()
    client.open()

    sub.open()

    client.post(b'hello', addr=3)

    m = await sub.recv()

    assert m.type == m.Type.Control

    m = await sub.recv()

    assert m.type == m.Type.Data
    assert m.data.tobytes() == b'hello'

    sub.post(b'hello', addr=m.addr)
    await check_response(client, 3, {'code':200}, b'hello')

@asyncloop_run
@pytest.mark.parametrize("send,recv", [('UNDEFINED', 'POST'), ('POST', 'POST'), ('GET', 'GET')])
async def test_http_method(asyncloop, server, port, send, recv):
    client = asyncloop.Channel(f'curl+http://127.0.0.1:{port}/path', transfer='control', name='client', dump='yes', method='POST')

    sub = asyncloop.Channel("uws+http://path", master=server, name='server/http', dump='yes');

    server.open()
    client.open()

    sub.open()

    msg = {'path': f'', 'method': send, 'size': 3}
    client.post(msg, name='Connect', type=client.Type.Control)
    client.post(b'xxx')

    m = await sub.recv()
    assert m.type == m.Type.Control
    m = sub.unpack(m)
    assert (m.path, m.method) == ('/path', getattr(m.method, recv))

def test_fail_open(context):
    c = context.Channel('uws://127.0.4.0:5010;mode=server;name=master', scheme='xxx')
    with pytest.raises(TLLError): c.open()

@asyncloop_run
async def test_http_wildcard(asyncloop, server, client):
    sub = asyncloop.Channel("uws+http://path", master=server, name='server/path', dump='yes');
    wc = asyncloop.Channel("uws+http://pa*", master=server, name='server/wildcard', dump='yes');
    other = asyncloop.Channel("uws+http://pb*", master=server, name='server/wildcard', dump='yes');

    server.open()
    client.open()

    client.post({'path':'/path/abc'}, type=client.Type.Control, name='Connect', addr=1)
    await check_response(client, 1, {'code':404})

    for c in [wc, sub]:
        c.open()

        client.post({'path':'/abc'}, type=client.Type.Control, name='Connect', addr=2)
        await check_response(client, 2, {'code':404})

        client.post({'path':'/path'}, type=client.Type.Control, name='Connect', addr=3)

        m = await c.recv()

        assert m.type == m.Type.Control
        assert c.unpack(m).path == '/path'

        m = await c.recv()

        assert m.type == m.Type.Data
        assert m.data.tobytes() == b''

        c.post(b'hello', addr=m.addr)

        await check_response(client, 3, {'code':200}, b'hello')

    other.open()
    client.post({'path':'/path/abc'}, type=client.Type.Control, name='Connect', addr=4)

    m = await wc.recv()

    assert m.type == m.Type.Control
    assert wc.unpack(m).path == '/path/abc'

    m = await wc.recv()

    assert m.type == m.Type.Data
    assert m.data.tobytes() == b''

    wc.post(b'hello', addr=m.addr)

    await check_response(client, 4, {'code':200}, b'hello')
