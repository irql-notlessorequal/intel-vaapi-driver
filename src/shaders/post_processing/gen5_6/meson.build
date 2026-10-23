# Mirrors the gen4asm suffix rules of Makefile.am (HAVE_GEN4ASM):
#
#   <kernel>.asm                           cpp + gpp.py       <kernel>.g5s
#   <kernel>.g5s                           gen4asm -a -o -g 5 <kernel>.g4b.gen5
#   <kernel>.asm                           cpp + gpp.py       <kernel>.g6s
#   <kernel>.g6s                           gen4asm -a -o -g 6 <kernel>.g6b
#   null.g4a                               m4                 null.g4m
#   null.g4m                               gen4asm -o -g 5    null.g4b.gen5
#   null.g6a                               m4                 null.g6m
#   null.g6m                               gen4asm -o -g 6    null.g6b
#
# Preprocessed files use DEV_ILK for gen5 and GT for gen6, so each
# generation must run its own cpp pass. The generated binaries are not
# consumed by the meson library build (the shipped files checked into the
# source tree are used instead); they are built explicitly, either per
# target name ('ninja <kernel>.g4b.gen5') or synced into the source tree
# via 'ninja regen-shaders'.

gen4asm_pkg = dependency(
  'intel-gen4asm',
  version : '>= 1.9',
  required : get_option('with_gen4asm') == 'yes')

gen4asm = find_program(
  'intel-gen4asm',
  required : get_option('with_gen4asm') == 'yes')

if gen4asm_pkg.found() and gen4asm.found()
  sh = find_program('sh')
  cpp = find_program('cpp')
  m4 = find_program('m4')
  python3 = find_program('python3')

  common_dir = join_paths(meson.current_source_dir(), 'Common')
  core_dir = join_paths(meson.current_source_dir(), 'Core_Kernels')
  gpp = join_paths(meson.project_source_root(), 'src', 'shaders', 'gpp.py')

  pp_pre_script = '''
  set -e
  tmp="@OUTDIR@/@0@_pp0"
  cpp -D @1@ -I "@2@/" -I "@3@/" "@INPUT@" > "$tmp"
  @4@ "@5@" "$tmp" "@OUTPUT@"
  rm -f "$tmp"
  '''

  pp_kernels = [
    'nv12_avs_nv12',
    'nv12_dn_nv12',
    'nv12_dndi_nv12',
    'nv12_load_save_nv12',
    'nv12_load_save_pa',
    'nv12_load_save_pl3',
    'nv12_load_save_rgbx',
    'nv12_scaling_nv12',
    'pa_load_save_pa',
    'pa_load_save_nv12',
    'pa_load_save_pl3',
    'pl3_load_save_nv12',
    'pl3_load_save_pa',
    'pl3_load_save_pl3',
    'rgbx_load_save_nv12',
  ]

  pp_gens = [
    [ 'DEV_ILK', 'g5s', 'g4b.gen5', '5' ],
    [ 'GT', 'g6s', 'g6b', '6' ],
  ]

  pp_binaries = []

  foreach gen : pp_gens
    foreach kernel : pp_kernels
      pp_pre = custom_target(
        '@0@-@1@-pre'.format(kernel, gen[0]),
        input : '@0@.asm'.format(kernel),
        output : '@0@.@1@'.format(kernel, gen[1]),
        command : [ sh, '-c',
                    pp_pre_script.format(kernel, gen[0], common_dir, core_dir,
                                         python3.path(), gpp) ])

      pp_bin = custom_target(
        '@0@-@1@'.format(kernel, gen[2]),
        input : pp_pre,
        output : '@0@.@1@'.format(kernel, gen[2]),
        command : [ gen4asm, '-a', '-o', '@OUTPUT@', '-g', gen[3], '@INPUT@' ])

      pp_binaries += [ pp_bin ]
    endforeach
  endforeach

  null_g4m = custom_target(
    'null_g4m',
    input : 'null.g4a',
    output : 'null.g4m',
    command : [ sh, '-c', 'm4 "@INPUT@" > "@OUTPUT@"' ])

  null_g4b_gen5 = custom_target(
    'null_g4b_gen5',
    input : null_g4m,
    output : 'null.g4b.gen5',
    command : [ gen4asm, '-g', '5', '-o', '@OUTPUT@', '@INPUT@' ])

  null_g6m = custom_target(
    'null_g6m',
    input : 'null.g6a',
    output : 'null.g6m',
    command : [ sh, '-c', 'm4 "@INPUT@" > "@OUTPUT@"' ])

  null_g6b = custom_target(
    'null_g6b',
    input : null_g6m,
    output : 'null.g6b',
    command : [ gen4asm, '-g', '6', '-o', '@OUTPUT@', '@INPUT@' ])

  pp_binaries += [ null_g4b_gen5, null_g6b ]

  run_target(
    'regen-shaders',
    command : [ sh, '-c',
        'cp -f "@0@/"*.g4b.gen5 "@0@/"*.g6b "@1@/"'.format(
            meson.current_build_dir(), meson.current_source_dir()) ],
    depends : pp_binaries)
endif
