project('tll-ws', 'c', 'cpp', version: '0.0.0', default_options: ['cpp_std=c++17', 'c_std=c11', 'werror=true', 'optimization=2', 'b_lto=true'])

cc = meson.get_compiler('c')
if cc.get_id() == 'gcc' and cc.version().version_compare('< 8.0')
        add_project_arguments('-Wno-unused-variable', language: 'cpp')
endif

include = [include_directories('src')]

tll = dependency('tll')
fmt = dependency('fmt')
lws = dependency('libwebsockets')
libuv = dependency('libuv')
libev = meson.get_compiler('c').find_library('ev')

lib = shared_library('tll-ws',
		['src/channel.cc', 'src/names.cc', 'src/ev-backend.c'],
		include_directories : include,
		dependencies : [fmt, lws, tll, libev],
		install : true
)

subdir('third_party')

uws = shared_library('tll-uws',
		['src/uws.cc'],
		include_directories : include,
		dependencies : [fmt, tll, libuv, uwebsockets],
		install : true
)

test('pytest', import('python').find_installation('python3')
	, args: ['-m', 'pytest', '-vx', 'test.py']
	, env: 'BUILD_DIR=@0@'.format(meson.current_build_dir())
	, workdir: meson.current_source_dir()
)
