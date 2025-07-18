#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};


MODULE_INFO(depends, "snd-pcm,snd");

MODULE_ALIAS("usb:v0D8Cp0013d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "5614392C3A46E066129C13B");
