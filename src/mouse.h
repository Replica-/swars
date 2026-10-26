#ifndef MOUSE_H
#define MOUSE_H

#include "bftypes.h"

struct TbPoint;

TbResult load_mouse_pointers_sprites_for_current_mode(void);

void mouse_setup(TbBool show_pointer);

void do_change_mouse(ushort mouse);

void mouse_update_on_screen_mode_change(TbBool show_pointer);

#endif
