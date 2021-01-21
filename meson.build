project('tll-ws', 'c', 'cpp', version: '0.0.0', default_options: ['cpp_std=c++17', 'werror=true', 'optimization=2'])

include = [include_directories('src')]

tll = dependency('tll')
fmt = dependency('fmt')
lws = dependency('libwebsockets')
#libuv = dependency('libuv')
libev = meson.get_compiler('c').find_library('ev')

lib = shared_library('tll-ws',
		['src/channel.cc', 'src/names.cc', 'src/ev-backend.c'],
		include_directories : include,
		dependencies : [fmt, lws, tll, libev],
		install : true)

test('pytest', import('python').find_installation('python3')
	, args: ['-m', 'pytest', '-vx', 'test.py']
	, env: 'BUILD_DIR=@0@'.format(meson.current_build_dir())
	, workdir: meson.current_source_dir()
)
