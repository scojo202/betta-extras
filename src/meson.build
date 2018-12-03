src_header_subdir = join_paths('y-extras','src')

src_public_headers = []
src_public_sources = []

src_public_headers += [
  'y-extras.h',
  'y-linear-range.h',
  'y-scalar-property.h',
  'y-vector-ring.h',
  'y-data-derived.h',
  'y-operation.h',
  'y-slice-operation.h',
  'y-hdf.h',
  'y-fft-operation.h',
  'y-simple-operation.h',
  'y-subset-operation.h'
]

src_public_sources += [
  'y-linear-range.c',
  'y-scalar-property.c',
  'y-vector-ring.c',
  'y-data-derived.c',
  'y-operation.c',
  'y-slice-operation.c',
  'y-hdf.c',
  'y-fft-operation.c',
  'y-simple-operation.c',
  'y-subset-operation.c'
]

install_headers(src_public_headers,subdir: 'libyextras-0.0')

yextras_deps = [libgobj_dep, libgio_dep, yplot_dep, fftw_dep, libm, hdf5]

libyextras = shared_library('yextras-0.0',src_public_sources, dependencies: yextras_deps, install: true, install_dir: get_option('libdir'))

libyextras_dep = declare_dependency(dependencies: yextras_deps, link_with: libyextras, include_directories: include_directories('.'),)

yextras_dep_sources = []

pkgg = import('pkgconfig')

pkgg.generate(
    libraries: [libyextras],
      subdirs: [ 'libyextras-0.0'.format(meson.project_version()) ],
      version: meson.project_version(),
         name: 'y-extras',
     filebase: 'y-extras-0.0',
  description: 'YExtras',
     requires: [ 'glib-2.0', 'gobject-2.0', 'gio-2.0', 'y-plot-0.2', 'fftw3'],
  install_dir: join_paths(get_option('libdir'), 'pkgconfig'),
)

# Introspection
if build_gir
  gir_extra_args = [
    '--quiet',
    '--warn-all',
  ]

  yextras_gir = gnome.generate_gir(libyextras,
                                    sources: src_public_headers + src_public_sources,
                                    namespace: 'YExtras',
                                    nsversion: '0.0',
                                    identifier_prefix: 'Y',
                                    symbol_prefix: 'y',
                                    export_packages: 'yextras',
                                    includes: [ 'GObject-2.0', 'Gio-2.0' ],
                                    install: true,
                                    extra_args: gir_extra_args)
  yextras_dep_sources += yextras_gir
endif
