# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('eslr', ['core','internet','network'])
    module.source = [
        'model/eslr-headers.cc',
        'model/eslr-route.cc',
        'model/eslr-neighbor.cc',
				'model/eslr-main.cc',
        'helper/eslr-helper.cc',
        ]

#    module_test = bld.create_ns3_module_test_library('eslr')
#    module_test.source = [
#        'test/eslr-test-suite.cc',
#        ]

    headers = bld(features='ns3header')
    headers.module = 'eslr'
    headers.source = [
        'model/eslr-definition.h',
        'model/eslr-headers.h',
        'model/eslr-route.h',
        'model/eslr-neighbor.h',
        'model/eslr-main.h',
        'helper/eslr-helper.h',
				]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

