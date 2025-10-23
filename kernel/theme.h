#pragma once

#include "../ui/theme.h"

void theme_init(void);
const ui_theme_t* theme_current(void);
ui_theme_id_t theme_current_id(void);
int theme_set(ui_theme_id_t id);
