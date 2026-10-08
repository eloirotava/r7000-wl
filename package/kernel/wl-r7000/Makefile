include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=wl-r7000
PKG_VERSION:=7.14.164.18
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define KernelPackage/wl-r7000
  SUBMENU:=Wireless Drivers
  TITLE:=Broadcom wl 7.14 (BCM4360) com nucleo binario do DD-WRT
  DEPENDS:=@TARGET_bcm53xx +kmod-nf-conntrack
  FILES:=$(PKG_BUILD_DIR)/wl.ko
endef

define KernelPackage/wl-r7000/description
  Driver wl da Broadcom para os BCM4360 do Netgear R7000.  A arvore
  Broadcom (src/brcm) e o nucleo juntado (src/core/wl_core.o) sao
  preparados fora daqui por scripts/fetch-brcm.sh e scripts/prep-core.sh.
  Exige o patch 990-skbuff-bcm-wl-abi no kernel.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
	test -f $(PKG_BUILD_DIR)/core/wl_core.o || \
		{ echo "falta src/core/wl_core.o (rode scripts/prep-core.sh)"; false; }
endef

define Build/Compile
	$(KERNEL_MAKE) M=$(PKG_BUILD_DIR) modules
endef

$(eval $(call KernelPackage,wl-r7000))
