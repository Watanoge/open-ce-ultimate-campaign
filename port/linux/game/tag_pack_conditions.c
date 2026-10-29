/*
TAG_PACK_CONDITIONS.C

The names a tag pack's manifest may want it by at start-up
("condition:<name>"), beside "always" and "setting:<key>" (tag_pack.c):
each a question only code can answer. This fork's; another build lists its
own here.
*/

#include "cseries.h"

#include "tag_pack.h"

/* skulls.c: skulls to be found (game.skulls on, unlocking by playing) or
their question not answered yet */
extern int skulls_pack_wanted(void);

struct tag_pack_condition const tag_pack_conditions[] =
{
	{ "skulls", skulls_pack_wanted },
	{ NULL, NULL },
};
