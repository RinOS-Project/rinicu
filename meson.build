project('RinICU', ['c', 'cpp'], version: '1.0.0',
        default_options: ['c_std=c11', 'cpp_std=c++17', 'warning_level=3'])
fs = import('fs')

sdk_inc = get_option('rinicu_sdk_include')
if sdk_inc == ''
  sdk_inc = meson.current_source_dir() / '../../RinOS-SDK/include'
endif
if not fs.exists(sdk_inc / 'rin/icu/service_abi.h')
  error('RinICU requires RinOS-SDK/include/rin/icu/service_abi.h')
endif
if host_machine.system() == 'windows'
  error('RinICU local-socket transport requires a POSIX/RinOS socket ABI')
endif

inc = include_directories('include', sdk_inc)
client_args = []
if get_option('rinicu_enable_autostart')
  client_args += ['-DRIN_ICU_ENABLE_AUTOSTART=1']
endif
rinicu = library('RinICU', 'src/client.c', include_directories: inc,
                 c_args: client_args, install: true, version: '1.0.0')
rinicu_dep = declare_dependency(link_with: rinicu, include_directories: inc)
install_subdir('include/rinicu', install_dir: get_option('includedir'))
pc = configure_file(input: 'rinicu.pc.in', output: 'rinicu.pc',
                    configuration: {'CMAKE_INSTALL_PREFIX': get_option('prefix'),
                                    'CMAKE_INSTALL_LIBDIR': get_option('libdir'),
                                    'CMAKE_INSTALL_INCLUDEDIR': get_option('includedir'),
                                    'PROJECT_VERSION': meson.project_version()})
install_data(pc, install_dir: get_option('libdir') / 'pkgconfig')

if get_option('buildtype') != 'plain'
  test('rinicu-abi', executable('rinicu-abi-test', 'tests/abi_contract_test.c',
        dependencies: rinicu_dep))
  test('rinicu-protocol', executable('rinicu-protocol-test',
        'tests/protocol_contract_test.c', dependencies: rinicu_dep))
  test('rinicu-data-policy', executable('rinicu-data-policy-test',
        'tests/data_policy_test.c', dependencies: rinicu_dep))
  test('rinicu-cpp-abi', executable('rinicu-cpp-abi-test',
        'tests/cpp_abi_compile_test.cpp', dependencies: rinicu_dep))
  test('rinicu-client-validation', executable('rinicu-client-validation-test',
        'tests/client_validation_test.c', include_directories: inc))
endif
