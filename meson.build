project('defi', 'cpp',
  version : '0.1',
  default_options : ['warning_level=3',
                     'cpp_std=c++20'])

src = files([
'./src/defi/uint64/orderbook.cpp',
'./src/defi/uint64/matcher.cpp',
'src/general/funds.cpp',
'src/defi/defi.cpp'
])

json = subproject('json')
json_dep = json.get_variable('nlohmann_json_dep')

if get_option('emscripten')
exe = executable('demo', ['src/demo.cpp',src],
  dependencies: json_dep,
  cpp_args:['-fexceptions',],
  include_directories: ['src'],
  link_args: ['-sMODULARIZE', '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToNewUTF8', '-sEXPORT_ES6',
    '-sNO_DISABLE_EXCEPTION_CATCHING', 
    '-sINITIAL_MEMORY=67108864'
    ],
  install : true)
else
exe = executable('defi', ['src/defi.cpp',src],
  include_directories: ['src'],
  dependencies: json_dep,
  install : true)
endif


test('basic', exe)
