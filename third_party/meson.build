libuv = dependency('libuv')
zlib = dependency('zlib')

usockets_src = files(
	'usockets/src/bsd.c',
	'usockets/src/context.c',
	'usockets/src/eventing/libuv.c',
	'usockets/src/socket.c',
	'usockets/src/loop.c',
)

usockets_include = include_directories('usockets/src')
usockets_lib = static_library('usockets',
		usockets_src,
		c_args : ['-DLIBUS_NO_SSL', '-DLIBUS_USE_LIBUV'],
		include_directories : usockets_include,
		install : false
)

usockets = declare_dependency(include_directories: usockets_include, link_with: usockets_lib, dependencies: [libuv, zlib])
uwebsockets = declare_dependency(include_directories: include_directories('uwebsockets/src'), dependencies: [usockets])
