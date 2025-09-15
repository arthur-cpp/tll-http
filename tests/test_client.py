#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import decorator
import os
import pytest

from tll import asynctll
from tll.channel import Context
from tll.test_util import ports

@pytest.fixture
def context():
    ctx = Context()
    try:
        ctx.load(os.path.join(os.environ.get("BUILD_DIR", "build"), "tll-uws"))
        ctx.load(os.path.join(os.environ.get("BUILD_DIR", "build"), "tll-uwsc"))
    except:
        pytest.skip("uws:// or ws:// channels not available")
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
    return ports.TCP4

@decorator.decorator
def asyncloop_run(f, asyncloop, *a, **kw):
    asyncloop.run(f(asyncloop, *a, **kw))

@pytest.fixture(scope='function', params=['yes', 'no'])
def client(asyncloop, port, request):
    c = asyncloop.Channel(f'ws://127.0.0.1:{port}/path', binary=request.param, name='client', dump='yes', **{'header.X-A': 'a', 'header.X-B': 'b'})
    yield c
    c.close()
    del c

@asyncloop_run
async def test(asyncloop, server, client, port):
    sub = asyncloop.Channel("uws+ws://path", master=server, name='server/ws', dump='yes');

    server.open()
    client.open(**{'header.X-A': 'zzz'})

    assert await client.recv_state() == client.State.Error

    client.close()
    sub.open()
    client.open(**{'header.X-A': 'Aa'})

    assert await client.recv_state() == client.State.Active
    assert client.state == client.State.Active

    assert client.config['info.local.af'] == 'ipv4'
    assert client.config['info.local.host'] == '127.0.0.1'
    assert client.config['info.remote.af'] == 'ipv4'
    assert client.config['info.remote.host'] == '127.0.0.1'
    assert client.config['info.remote.port'] == f'{port}'

    m = await sub.recv(0.1)
    assert m.type == m.Type.Control
    m = sub.unpack(m)
    assert m.SCHEME.name == 'Connect'

    client.post(b'xxx')
    client.post(b'yyy')

    m = await sub.recv(0.1)
    assert m.type == m.Type.Data
    assert m.data.tobytes() == b'xxx'

    m = await sub.recv(0.1)
    assert m.type == m.Type.Data
    assert m.data.tobytes() == b'yyy'

    sub.post(b'zzz', addr=m.addr)

    m = await client.recv(0.1)
    assert m.type == m.Type.Data
    assert m.data.tobytes() == b'zzz'

    client.close()

    m = await sub.recv(0.1)
    assert m.type == m.Type.Control
    m = sub.unpack(m)
    assert m.SCHEME.name == 'Disconnect'
