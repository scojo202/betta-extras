project('b-extras','c', version: '0.0.1', default_options: [
                     'c_std=gnu11'])

gnome = import('gnome')

libgio_dep = dependency('gio-2.0', version: '>= 2.52')
libgobj_dep = dependency('gobject-2.0', version: '>= 2.52')
libbetta_dep = dependency('libbetta-0.2', version: '>= 0.1.3')
fftw_dep = dependency('fftw3', version: '>=3.2')
aravis_dep = dependency('aravis-0.8', required : false)
png_dep = dependency('libpng')

comp = meson.get_compiler('c')
libm = comp.find_library('m', required: false)
hdf5 = [comp.find_library('hdf5_hl', required: true),comp.find_library('hdf5', required: true)]

conf = configuration_data()
conf.set_quoted('PACKAGE_NAME', 'b-extras')

add_project_arguments('-Wall', language : 'c')

if aravis_dep.found()
add_global_arguments('-DARAVIS', language : 'c')
endif

if get_option('enable-introspection')
  gir = find_program('g-ir-scanner', required: false)
  if get_option('default_library') == 'shared'
    build_gir = gir.found() and not meson.is_cross_build()
  else
    build_gir = false
  endif
else
    build_gir = false
endif

python3 = import('python3')

subdir('data')
subdir('src')
subdir('test')

if get_option('enable-gtk-doc')
  subdir('docs/reference')
endif
