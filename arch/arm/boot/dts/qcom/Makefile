#ASUS BSP Jason2_zhang +++
dtb-$(CONFIG_ARCH_MSM8937) += msm8937-pmi8950-mtp.dtb \
	ZC551KL-SR-msm8937-pmi8950-mtp.dtb \
	ZC551KL-ER1-msm8937-pmi8950-mtp.dtb \
	ZC551KL-ER2-msm8937-pmi8950-mtp.dtb \
	ZC551KL-ER3-msm8937-pmi8950-mtp.dtb \
	ZC551KL-MP-msm8937-pmi8950-mtp.dtb \
	ZC600KL-EVB-msm8937-pmi8950-mtp.dtb \
	ZC551KL-ER1-msm8917-pmi8950-mtp.dtb
#ASUS BSP Jason2_zhang ---

ifeq ($(CONFIG_ARM64),y)
always          := $(dtb-y)
subdir-y        := $(dts-dirs)
else
targets += dtbs
targets += $(addprefix ../, $(dtb-y))

$(obj)/../%.dtb: $(src)/%.dts FORCE
        $(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/,$(dtb-y))
endif
clean-files := *.dtb
