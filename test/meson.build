test_cflags = [
  '-I'+meson.source_root()+'/src',
]

testdata = executable('testdata',
  'testdata.c',
  c_args : test_cflags,
  link_args : ['-lm'],
  dependencies: [
    libbextras_dep
  ],
)

testhdf5 = executable('testhdf5',
  'testhdf5.c',
  c_args : test_cflags,
  link_args : ['-lm'],
  dependencies: [
    libbextras_dep
  ],
)

testsignals = executable('testsignals',
  'testsignals.c',
  c_args : test_cflags,
  link_args : ['-lm'],
  dependencies: [
    libbextras_dep
  ],
)

meson.source_root()+'/src'
