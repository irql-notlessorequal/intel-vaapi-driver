# Copyright © 2017 Intel Corporation
# SPDX-License-Identifier: MIT

gen_xml_files = [
  'gen40.xml',
  'gen45.xml',
  'gen50.xml',
  'gen60.xml',
  'gen70.xml',
  'gen75.xml',
  'gen80.xml',
  'gen90.xml',
#  'gen100.xml', # XXX - Intel changed the XML format since then, will need to convert it over -
]

gen_pack_header_deps = files('intel_genxml.py', 'util.py')

genX_xml_h = custom_target(
  'genX_xml.h',
  input : ['gen_zipped_xml_file.py', gen_xml_files],
  output : 'genX_xml.h',
  command : [prog_python, '@INPUT@'],
  capture : true,
)

genX_bits_included_symbols = [
  # instructions
  'MI_BATCH_BUFFER_START::Batch Buffer Start Address',
  'MI_REPORT_PERF_COUNT::Memory Address',
  'MI_STORE_DATA_IMM::Address',
  'MI_STORE_DATA_IMM::Immediate Data',
  'MI_STORE_REGISTER_MEM::Memory Address',
  '3DSTATE_DEPTH_BUFFER::Surface Base Address',
  '3DSTATE_DEPTH_BUFFER::Surface Pitch',
  '3DSTATE_STENCIL_BUFFER::Surface Base Address',
  '3DSTATE_STENCIL_BUFFER::Surface Pitch',
  '3DSTATE_HIER_DEPTH_BUFFER::Surface Base Address',
  '3DSTATE_HIER_DEPTH_BUFFER::Surface Pitch',
  '3DSTATE_CLEAR_PARAMS',
  '3DSTATE_SO_BUFFER::Surface Base Address',
  '3DSTATE_SO_BUFFER::Stream Offset',
  # structures
  'RENDER_SURFACE_STATE::Surface Base Address',
  'RENDER_SURFACE_STATE::Surface Pitch',
  'RENDER_SURFACE_STATE::Surface QPitch',
  'RENDER_SURFACE_STATE::Auxiliary Surface Base Address',
  'RENDER_SURFACE_STATE::Auxiliary Surface Pitch',
  #'RENDER_SURFACE_STATE::Clear Value Address', -- GEN100 ONLY
  'RENDER_SURFACE_STATE::Red Clear Color',
  'RENDER_SURFACE_STATE::Green Clear Color',
  'RENDER_SURFACE_STATE::Blue Clear Color',
  'RENDER_SURFACE_STATE::Alpha Clear Color',
  'RENDER_SURFACE_STATE::Width',
  'RENDER_SURFACE_STATE::Height',
  'RENDER_SURFACE_STATE::Depth',
  'RENDER_SURFACE_STATE::Surface Format',
  'RENDER_SURFACE_STATE::Surface Type',
  'RENDER_SURFACE_STATE::Render Target View Extent',
  'RENDER_SURFACE_STATE::Tile Mode',
  'VERTEX_BUFFER_STATE::Buffer Starting Address'
]

genX_bits_h = custom_target(
  'genX_bits.h',
  input : ['gen_bits_header.py', gen_xml_files],
  output : 'genX_bits.h',
  command : [prog_python, '@INPUT@', '-o', '@OUTPUT@',
             '--include-symbols', ','.join(genX_bits_included_symbols)],
  depend_files: gen_pack_header_deps
)

gen_xml_pack = []
foreach f : gen_xml_files
  _name = '@0@_pack.h'.format(f.split('.')[0])
  gen_xml_pack += custom_target(
    _name,
    input : ['gen_pack_header.py', f],
    output : _name,
    command : [prog_python, '@INPUT@', '--engines=render,blitter,compute'],
    capture : true,
    depend_files: gen_pack_header_deps
  )
endforeach

gen_xml_video_pack = []
foreach f : gen_xml_files
  _name = '@0@_video_pack.h'.format(f.split('.')[0])
  gen_xml_pack += custom_target(
    _name,
    input : ['gen_pack_header.py', f],
    output : _name,
    command : [prog_python, '@INPUT@', '--engines=video'],
    capture : true,
    depend_files: gen_pack_header_deps
  )
endforeach

gen_pack_header_py = files('gen_pack_header.py')

idep_genxml = declare_dependency(
  include_directories : include_directories('.'),
 # dependencies : idep_mesautil,
  sources : [gen_xml_pack, gen_xml_video_pack, genX_bits_h, genX_xml_h]
)