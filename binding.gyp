{
  'variables': {
    'enable_asan%': 'false',
    'disable_int%': 'false'
  },
  'conditions': [
    ['disable_int == "true"', {
      'defines': [ 'EXPRTK_DISABLE_INT_TYPES' ]
    }]
  ],
  'target_defaults': {
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
        'src/expression.cc'
      ],
      'include_dirs': [
        'deps/exprtk',
        '<!@(node -p \'require("node-addon-api").include\')'
      ],
      'defines': [
        'exprtk_disable_string_capabilities'
      ],
      'dependencies': ['<!(node -p \'require("node-addon-api").gyp\')'],
      'cflags!': [ '-fno-exceptions', '-fno-rtti', '-fvisibility=default' ],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti', '-fvisibility=default' ],
      'cflags_cc': [ '-fvisibility=hidden' ],
      'ldflags': [ '-Wl,-z,now' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'CLANG_CXX_LIBRARY': 'libc++',
        'MACOSX_DEPLOYMENT_TARGET': '10.7'
      },
      'msvs_settings': {
        'VCCLCompilerTool': { 'ExceptionHandling': 1 },
      },
      'conditions': [
        ['enable_asan == "true"', {
          'variables': {
            'asan': 1
          },
          'cflags_cc': [ '-fsanitize=address' ],
          'ldflags' : [ '-fsanitize=address' ]
        }]
      ]
    },
    {
      'target_name': 'action_after_build',
      'type': 'none',
      'dependencies': [ '<(module_name)' ],
      'copies': [
        {
          'files': [
            '<(PRODUCT_DIR)/exprtk.js.node'
          ],
          'destination': '<(module_path)'
        }
      ]
    }
  ]
}