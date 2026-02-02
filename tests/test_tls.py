#!/usr/bin/env python
# vim: sts=4 sw=4 et

import decorator
import os
import pytest

import websockets
import ssl

from tll.asynctll import asyncio as asynctll
from tll.channel import Context
from tll.test_util import ports

@decorator.decorator
def asyncloop_run(f, asyncloop, *a, **kw):
    asyncloop.run(f(asyncloop, *a, **kw))

@pytest.fixture
def asyncloop(context):
    loop = asynctll.Loop(context)
    yield loop
    loop.destroy()
    loop = None

@pytest.fixture
def port():
    return ports.TCP4

@pytest.fixture
def context():
    ctx = Context()
    try:
        ctx.load(os.path.join(os.environ.get("BUILD_DIR", "build"), "tll-uwsc"))
    except:
        pytest.skip("ws:// channel not available")
    return ctx

@pytest.fixture
def ssl_context():
    if int(websockets.version.version.split('.')[0]) < 10:
        pytest.skip(f"Websockets module version too old: {websockets.version.version}, need at least 10.0")
    r = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    r.load_cert_chain('cert/server.pem', keyfile='cert/server.key')
    return r

@asyncloop_run
async def test(asyncloop, port, ssl_context):
    async with websockets.serve(echo, "*", port, ssl=ssl_context) as server:
        client = asyncloop.Channel(f"wss://127.0.0.1:{port}", name='client', dump='yes', ca='cert/ca.pem');
        try:
            client.open()
            assert await client.recv_state(0.1) == client.State.Active

            client.post(b'xxx')

            m = await client.recv(0.1)
            assert (m.type, m.data.tobytes()) == (m.Type.Data, b'xxx')
        finally:
            client.close()

async def echo(sock):
    async for message in sock:
        await sock.send(message)
