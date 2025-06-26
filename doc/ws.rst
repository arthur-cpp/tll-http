tll-channel-ws
==============

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Websocket client channel

Synopsis
--------

``ws://ADDRESS;ping=<duration>;binary=<bool>``

``wss://ADDRESS;...``


Description
-----------

Channel implements Websocket both unencrypted (``ws://``) and encrypted
(``wss://``) client using libuwsc lightweight library.

Init parameters
~~~~~~~~~~~~~~~

``ADDRESS`` - websocket endpoint like ``example.com/path?param``. Passed to server in HTTP header.

``binary=<bool>`` (default ``yes``) - send data as binary frames or as text.

``ping=<duration>`` (default ``3s``) - ping interval

``header.**=<value>`` - additional headers included in initial request,
``header.`` substring is stripped.

Open parameters
~~~~~~~~~~~~~~~

``header.**=<string>`` - same as init parameter (overrides if same header is
specified) but only for one session.

Config info
~~~~~~~~~~~

When channel enters Active state local and remote network addresses are exported in config info
subtree under ``local`` and ``remote`` nodes:

  - ``af`` - address family, one of ``ipv4`` or ``ipv6``.
  - ``host`` - ip address
  - ``port`` - tcp port

Examples
--------

Open secure websocket connection in text mode with additional header::

    wss://example.com/path;binary=no;header.X-Auth-Header=TOKEN

If auth token is changed often - pass it in open parameters::

    channel.open({'heaer.X-Auth-Header': 'TOKEN'})

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
