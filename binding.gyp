{
  'targets': [
    {
      'target_name': 'exprtk.js-native',
      'sources': [ 
        'src/exprtk_instance.cc',
        'src/expression.cc'
      ],
      'include_dirs': [
        "deps/exprtk",
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      'defines': [
        #'exprtk_disable_string_capabilities'
      ],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'cflags!': [ '-fno-exceptions', '-fno-rtti' ],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'CLANG_CXX_LIBRARY': 'libc++',
        'MACOSX_DEPLOYMENT_TARGET': '10.7'
      },
      'msvs_settings': {
        'VCCLCompilerTool': { 'ExceptionHandling': 1 },
      }
    }
  ]
}