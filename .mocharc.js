module.exports = {
  'spec': 'test/*.test.[tj]s',
  'require': [
    'ts-node/register',
    'tsconfig-paths/register'
  ],
  'timeout': '20000',
  'reporter': 'tap',
  'node-option':
    [
      ...((process.versions.node.split('.')[0] == 20 && process.versions.node.split('.')[1] >= 19) ? ['no-experimental-require-module'] : []),
      ...(process.versions.node.split('.')[0] >= 22 ? ['no-experimental-strip-types'] : []),
      'expose-gc'
    ]
};
