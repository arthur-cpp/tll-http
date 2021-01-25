#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import http.client
import os
import pytest
import threading
import urllib.request

from tll import asynctll
from tll.channel import Context

class Test:
    def setup(self):
        self.ctx = Context()
        self.ctx.load(os.path.join(os.environ.get("BUILD_DIR", "build"), "tll-uws"), 'channel_module')
        self.loop = asynctll.Loop(context=self.ctx)

        self.server = self.loop.Channel('ws://*:8080', name='server')
        self.thread_event = self.loop.Channel("direct://", name='thread/tll', dump='text')
        self.main_event = self.ctx.Channel("direct://", name='thread/main', master=self.thread_event)
        self.channels = []

        self.thread = None
        self.thread_stop = threading.Event()
        self.thread_error = None

        self.thread_event.open()
        self.main_event.open()

    def teardown(self):
        if self.thread:
            self.thread_stop.set()
            self.thread.join()
        self.thread = None
        self.thread_stop = None
        self.thread_error = None

        if self.loop:
            self.loop.stop = 1
            self.loop = None

        for c in self.channels + [self.thread_event, self.main_event]:
            c.close()

        self.thread_event = None
        self.main_event = None
        self.channels = []

        if self.server:
            self.server.close()
            self.server = None
        self.ctx = None

    def http(self, *a, **kw):
        conn = http.client.HTTPConnection('::1', 8080, timeout=1)
        conn.request(*a, **kw)
        return conn.getresponse()

    def test_http(self):
        sub = self.loop.Channel("ws+http://path", master=self.server, name='server/http', dump='yes');

        self.channels = [sub]

        self.server.open()

        def process(self, sub):
            async def main():
                m = await self.thread_event.recv()
                assert m.data.tobytes() == b'open'

                sub.open()

                m = await sub.recv()
                assert m.type == m.Type.Control

                sub.post(b'hello', addr=m.addr)

                m = await self.thread_event.recv()
                assert m.data.tobytes() == b'done'
            try:
                self.loop.run(main())
            except Exception as e:
                self.thread_error = e
                raise

        self.thread = threading.Thread(target=process, args=(self, sub))
        self.thread.start()

        r = self.http('GET', '/')
        assert r.status == 404
        assert self.thread_error == None, self.thread_error

        self.main_event.post(b'open')

        r = self.http('GET', '/')
        assert r.status == 404
        assert self.thread_error == None, self.thread_error

        r = self.http('GET', '/path')
        assert r.status == 200
        assert r.read() == b'hello'
        assert self.thread_error == None, self.thread_error

        self.main_event.post(b'done')
        self.loop.stop = 1

        self.thread.join()
        assert self.thread_error == None, self.thread_error
