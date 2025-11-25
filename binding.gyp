{
  'variables': {
    'enable_asan%': 'false',
    'enable_coverage%': 'false',
    'disable_int%': 'false'
  },
  'target_defaults': {
    'cflags!': [ '-fno-exceptions', '-fno-rtti', '-fvisibility=default' ],
    'cflags_cc!': [ '-fno-exceptions', '-fno-rtti', '-fvisibility=default' ],
    'cflags_cc': [ '-fvisibility=hidden', '-std=c++17' ],
    'ldflags': [ '-Wl,-z,now' ],
    'xcode_settings': {
      'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
			'GCC_ENABLE_CPP_RTTI': 'YES',
      'CLANG_CXX_LIBRARY': 'libc++',
      'MACOSX_DEPLOYMENT_TARGET': '13.0',
			'OTHER_CPLUSPLUSFLAGS': [
				'-frtti',
				'-fexceptions',
        '-std=c++17'
			]
    },
    'conditions': [
      ['disable_int == "true"', {
        'defines': [ 'EXPRTK_DISABLE_INT_TYPES' ]
      }]
    ],
    'msvs_settings': {
      'VCCLCompilerTool': {
        'AdditionalOptions': [
          '/MP',
          '/GR',
          '/EHsc',
          '/bigobj',
          '/wd4146',
          '/wd4723',
          '/std:c++17'
        ],
        'ExceptionHandling': 1,
        'RuntimeTypeInfo': 'true'
      }
    },
    'configurations': {
      'Debug': {
        'cflags_cc!': [ '-O3', '-Os' ],
        'defines': [ 
          'DEBUG',
          'exprtk_disable_enhanced_features'
        ],
        'defines!': [ 'NDEBUG' ],
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '0',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'YES',
        }
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'defines!': [ 'DEBUG' ],
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': 's',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
          'DEAD_CODE_STRIPPING': 'YES',
          'GCC_INLINES_ARE_PRIVATE_EXTERN': 'YES'
        },
        'ldflags': [
          '-Wl,-s'
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'DebugInformationFormat': '0',
          },
          'VCLinkerTool': {
            'GenerateDebugInformation': 'false',
          }
        }
      }
    }
  },
  'targets': [
    {
      'target_name': 'exprtk.js',
      'sources': [
        'src/expression.cc',
        'src/async.cc',
        'src/ndarray.cc'
      ],
      'include_dirs': [
        'deps/exprtk',
        'include',
        '<!@(node -p "require(\'node-addon-api\').include")'
      ],
      'defines': [
        'NAPI_VERSION=4',
        'exprtk_disable_string_capabilities',
        'exprtk_disable_rtl_io_file'
      ],
      'dependencies': ['<!(node -p "require(\'node-addon-api\').gyp")'],
      'conditions': [
        ['enable_asan == "true"', {
          'cflags_cc': [ '-fsanitize=address' ],
          'ldflags' : [ '-fsanitize=address' ]
        }],
        ["enable_coverage == 'true'", {
          'cflags_cc': [ '-fprofile-arcs', '-ftest-coverage' ],
          'ldflags' : [ '-lgcov', '--coverage' ]
        }]
      ]
    },
    {
      'target_name': 'exprtk.js-test',
      'sources': [
        'test/addon.test.cc'
      ],
      'include_dirs': [
        'include',
        '<!@(node -p "require(\'node-addon-api\').include")'
      ],
      'dependencies': ['<!(node -p "require(\'node-addon-api\').gyp")'],
    },
    {
      'target_name': 'action_after_build',
      'type': 'none',
      'dependencies': [ '<(module_name)' ],
      'copies': [
        {
          'files': [
            '<(PRODUCT_DIR)/exprtk.js.node',
            '<(PRODUCT_DIR)/exprtk.js-test.node'
          ],
          'destination': '<(module_path)'
        }
      ]
    }
  ]
}
