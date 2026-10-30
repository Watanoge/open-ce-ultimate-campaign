/* Android device policy, selected before the game starts and never changed
while resources exist. Unknown hardware retains both compatibility paths. */
#ifndef ANDROID_RENDER_POLICY_H
#define ANDROID_RENDER_POLICY_H

enum android_chip_family
{
	_android_chip_unknown,
	_android_chip_snapdragon,
	_android_chip_mediatek,
	_android_chip_other
};

int android_chip_classify(const char *manufacturer, const char *model,
	const char *hardware, const char *platform);

#if defined(HALO_WEB) && !defined(HALO_ANDROID)
/* (the web build takes Android's CE texture path always: it has no chip to
ask, xbox_texture_cache.c and xbox_textures.c) */
#define android_texture_fixes 1
#else
/* Guest state: written once on its main thread, before game threads start. */
extern int android_geometry_fixes;
extern int android_texture_fixes;
extern int android_render_chip;
void android_render_policy_initialize(void);
#endif

#endif
