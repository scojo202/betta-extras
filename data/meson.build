schema_dir = join_paths(get_option('datadir'), 'glib-2.0', 'schemas')

install_data([
  'com.github.scojo202.betta-extras.gschema.xml'], install_dir: schema_dir)

install_data('b-cam-viewer.desktop', install_dir: join_paths(get_option('datadir'),'applications'))

meson.add_install_script('compile_schemas.sh')

