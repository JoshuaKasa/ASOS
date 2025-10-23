#include "theme.h"
#include "console.h"

static ui_theme_id_t g_theme_id = UI_THEME_DARK;

void theme_init(void) {
    g_theme_id = UI_THEME_DARK;
    const ui_theme_t* theme = ui_theme_get(g_theme_id);
    console_on_theme_changed(theme);
}

const ui_theme_t* theme_current(void) {
    return ui_theme_get(g_theme_id);
}

ui_theme_id_t theme_current_id(void) {
    return g_theme_id;
}

int theme_set(ui_theme_id_t id) {
    if ((unsigned int)id >= UI_THEME_COUNT)
        return -1;

    g_theme_id = id;
    const ui_theme_t* theme = ui_theme_get(g_theme_id);
    console_on_theme_changed(theme);
    console_redraw();
    return 0;
}
