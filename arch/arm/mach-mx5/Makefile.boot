   zreladdr-$(CONFIG_SOC_IMX50)	:= 0x70008000
params_phys-$(CONFIG_SOC_IMX50)	:= 0x70000100
initrd_phys-$(CONFIG_SOC_IMX50)	:= 0x70800000
   zreladdr-$(CONFIG_SOC_IMX51)	:= 0x90008000
params_phys-$(CONFIG_SOC_IMX51)	:= 0x90000100
initrd_phys-$(CONFIG_SOC_IMX51)	:= 0x90800000
   zreladdr-$(CONFIG_SOC_IMX53)	:= 0x70008000
params_phys-$(CONFIG_SOC_IMX53)	:= 0x70000100
initrd_phys-$(CONFIG_SOC_IMX53)	:= 0x70800000

dtb-$(CONFIG_MACH_MX51_BABBAGE) += mx51-babbage.dtb
dtb-$(CONFIG_MACH_MX51_EFIKAMX) += genesi-efikamx.dtb
dtb-$(CONFIG_MACH_MX51_EFIKASB) += genesi-efikasb.dtb
dtb-$(CONFIG_MACH_MX53_LOCO) += mx53-loco.dtb
