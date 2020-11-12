src_header_subdir = join_paths('b-extras','src')

bextras_resource = gnome.compile_resources('betta-extras resources','b-extras.gresource.xml', source_dir: '.')

src_public_headers = []
src_public_sources = []

src_public_headers += [
  'b-extras.h',
  'b-scalar-property.h',
  'b-data-derived.h',
  'b-operation.h',
  'b-slice-operation.h',
  'b-hdf.h',
  'b-fft-operation.h',
  'b-simple-operation.h',
  'b-subset-operation.h',
  'b-image.h'
]

src_public_sources += [
  'b-scalar-property.c',
  'b-data-derived.c',
  'b-operation.c',
  'b-slice-operation.c',
  'b-hdf.c',
  'b-fft-operation.c',
  'b-simple-operation.c',
  'b-subset-operation.c',
  'b-image.c'
]

bextras_deps = [libgobj_dep, libgio_dep, libbetta_dep, fftw_dep, libm, hdf5, png_dep]

if aravis_dep.found()
  src_public_headers+=['b-arv-source.h','b-camera-settings-grid.h','b-video-window.h']
  src_public_sources+=['b-arv-source.c','b-camera-settings-grid.c','b-video-window.c']
  bextras_deps += aravis_dep
endif

install_headers(src_public_headers,subdir: 'libbextras-0.2')

libbextras = shared_library('bextras-0.2',src_public_sources + bextras_resource, dependencies: bextras_deps, install: true, install_dir: get_option('libdir'))

libbextras_dep = declare_dependency(dependencies: bextras_deps, link_with: libbextras, include_directories: include_directories('.'),)

if aravis_dep.found()
bcamviewer = executable('b-cam-viewer',
  ['b-cam-viewer.c'],
  dependencies: [
    libbetta_dep,
    libbextras_dep,
    aravis_dep
  ],
  install: true
)
endif

bextras_dep_sources = []

pkgg = import('pkgconfig')

pkg_reqs = [ 'glib-2.0', 'gobject-2.0', 'gio-2.0', 'libbetta-0.2', 'fftw3']

if aravis_dep.found()
  pkg_reqs += ['aravis-0.8']
endif

pkgg.generate(
    libraries: [libbextras],
      subdirs: [ 'libbextras-0.2'.format(meson.project_version()) ],
      version: meson.project_version(),
         name: 'b-extras',
     filebase: 'b-extras-0.2',
  description: 'BExtras',
     requires: pkg_reqs,
  install_dir: join_paths(get_option('libdir'), 'pkgconfig'),
)

# Introspection
if build_gir
  gir_extra_args = [
    '--quiet',
    '--warn-all',
  ]

  bextras_gir = gnome.generate_gir(libbextras,
                                    sources: src_public_headers + src_public_sources,
                                    namespace: 'BExtras',
                                    nsversion: '0.2',
                                    identifier_prefix: 'B',
                                    symbol_prefix: 'b',
                                    export_packages: 'bextras',
                                    includes: [ 'GObject-2.0', 'Gio-2.0' ],
                                    install: true,
                                    extra_args: gir_extra_args)
  bextras_dep_sources += bextras_gir
endif
