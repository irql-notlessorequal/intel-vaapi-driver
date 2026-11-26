SUBDIRS = gen5_6 gen7 gen75

if USE_GEN8_BDW_SUPPORT
SUBDIRS += gen8
else
if USE_GEN8_LP_SUPPORT
SUBDIRS += gen8
endif
endif

if USE_GEN9_SUPPORT
SUBDIRS += gen9
endif

DIST_SUBDIRS = gen5_6 gen7 gen75 gen8 gen9

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = Makefile.in
