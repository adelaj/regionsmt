.DEFAULT_GOAL = all

include mk/common.mk
include mk/env.mk
include mk/var.mk
include mk/gather.mk
include mk/build.mk
include mk/contrib.mk

TARGET := RegionsMT

$(call var_reg,$(addprefix -l,m pthread),$$1,LDFLAGS,$(TARGET),$(CC_TOOLCHAIN),%:%)
$(call var_reg,$$$$(PREFIX)/$$$$3/gsl/$$$$4/$$$$5.log,$$1,CREQ,$(TARGET),$(CC_TOOLCHAIN),%:%)
$(call var_reg,$$$$(addprefix $$$$(PREFIX)/$$$$3/gsl/$$$$4/$$$$5/,libgsl.a libgslcblas.a),$$1,LDREQ,$(TARGET),$(CC_TOOLCHAIN),%:%)

$(call var_reg,/W4,$$1,CFLAGS,$(TARGET),msvc:%:%)

.PHONY: all
all: $(call build,cc,$(TARGET),$(CC_MATRIX))

.PHONY: clean
clean: | $(call do_clean)

include $(wildcard $(call coalesce,INCLUDE,))