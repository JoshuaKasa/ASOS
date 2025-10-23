#pragma once

#include <stdint.h>

// Helper to compose VGA attributes
#define UI_ATTR(fg, bg) ((((uint8_t)(bg) & 0x0Fu) << 4) | ((uint8_t)(fg) & 0x0Fu))

// Logical color roles shared across apps and the kernel
typedef enum {
    UI_THEME_ROLE_CANVAS_BG = 0,
    UI_THEME_ROLE_PANEL_BG,
    UI_THEME_ROLE_PANEL_ACCENT,
    UI_THEME_ROLE_ACCENT,
    UI_THEME_ROLE_TEXT_PRIMARY,
    UI_THEME_ROLE_TEXT_MUTED,
    UI_THEME_ROLE_SUCCESS,
    UI_THEME_ROLE_WARNING,
    UI_THEME_ROLE_INPUT_BG,
    UI_THEME_ROLE_INPUT_TEXT,
    UI_THEME_ROLE_SELECTION,
    UI_THEME_ROLE_COUNT
} ui_theme_role_t;

// Available themes
typedef enum {
    UI_THEME_DARK = 0,
    UI_THEME_LIGHT = 1,
    UI_THEME_COUNT
} ui_theme_id_t;

typedef struct {
    const char* name;
    struct {
        uint32_t rgb;   // 0x00RRGGBB
        uint8_t  attr;  // VGA text attribute (4-bit fg/bg)
    } slots[UI_THEME_ROLE_COUNT];
} ui_theme_t;

static inline const ui_theme_t* ui_theme_table(void) {
    static const ui_theme_t kThemes[UI_THEME_COUNT] = {
        [UI_THEME_DARK] = {
            .name = "Dark",
            .slots = {
                [UI_THEME_ROLE_CANVAS_BG]     = { 0x122033u, UI_ATTR(0x0, 0x0) },
                [UI_THEME_ROLE_PANEL_BG]      = { 0x1B2B3Fu, UI_ATTR(0x7, 0x0) },
                [UI_THEME_ROLE_PANEL_ACCENT]  = { 0x253C5Cu, UI_ATTR(0xF, 0x1) },
                [UI_THEME_ROLE_ACCENT]        = { 0x4DB5FFu, UI_ATTR(0xB, 0x0) },
                [UI_THEME_ROLE_TEXT_PRIMARY]  = { 0xE6EEF5u, UI_ATTR(0xF, 0x0) },
                [UI_THEME_ROLE_TEXT_MUTED]    = { 0x9AAAC0u, UI_ATTR(0x8, 0x0) },
                [UI_THEME_ROLE_SUCCESS]       = { 0x7BD992u, UI_ATTR(0xA, 0x0) },
                [UI_THEME_ROLE_WARNING]       = { 0xFF7A7Au, UI_ATTR(0xC, 0x0) },
                [UI_THEME_ROLE_INPUT_BG]      = { 0x162536u, UI_ATTR(0xF, 0x1) },
                [UI_THEME_ROLE_INPUT_TEXT]    = { 0xFFFFFFu, UI_ATTR(0xF, 0x0) },
                [UI_THEME_ROLE_SELECTION]     = { 0x34506Fu, UI_ATTR(0xF, 0x1) },
            },
        },
        [UI_THEME_LIGHT] = {
            .name = "Light",
            .slots = {
                [UI_THEME_ROLE_CANVAS_BG]     = { 0xF3F6FAu, UI_ATTR(0x0, 0xF) },
                [UI_THEME_ROLE_PANEL_BG]      = { 0xFFFFFFu, UI_ATTR(0x0, 0xF) },
                [UI_THEME_ROLE_PANEL_ACCENT]  = { 0xD6E2F3u, UI_ATTR(0x1, 0xF) },
                [UI_THEME_ROLE_ACCENT]        = { 0x1F6AB5u, UI_ATTR(0x1, 0xF) },
                [UI_THEME_ROLE_TEXT_PRIMARY]  = { 0x1A232Eu, UI_ATTR(0x0, 0xF) },
                [UI_THEME_ROLE_TEXT_MUTED]    = { 0x5F6B7Cu, UI_ATTR(0x8, 0xF) },
                [UI_THEME_ROLE_SUCCESS]       = { 0x2E8B57u, UI_ATTR(0x2, 0xF) },
                [UI_THEME_ROLE_WARNING]       = { 0xD64545u, UI_ATTR(0x4, 0xF) },
                [UI_THEME_ROLE_INPUT_BG]      = { 0xFFFFFFu, UI_ATTR(0x0, 0xF) },
                [UI_THEME_ROLE_INPUT_TEXT]    = { 0x1A232Eu, UI_ATTR(0x0, 0xF) },
                [UI_THEME_ROLE_SELECTION]     = { 0xB0C8E4u, UI_ATTR(0xF, 0x1) },
            },
        },
    };
    return kThemes;
}

static inline const ui_theme_t* ui_theme_get(ui_theme_id_t id) {
    const ui_theme_t* table = ui_theme_table();
    if ((unsigned int)id >= UI_THEME_COUNT)
        return table + UI_THEME_DARK;
    return table + id;
}

static inline ui_theme_id_t ui_theme_default_id(void) {
    return UI_THEME_DARK;
}

static inline const char* ui_theme_name(ui_theme_id_t id) {
    return ui_theme_get(id)->name;
}

static inline uint32_t ui_theme_rgb_role(const ui_theme_t* theme, ui_theme_role_t role) {
    if (!theme || role >= UI_THEME_ROLE_COUNT)
        return 0;
    return theme->slots[role].rgb;
}

static inline uint8_t ui_theme_attr_role(const ui_theme_t* theme, ui_theme_role_t role) {
    if (!theme || role >= UI_THEME_ROLE_COUNT)
        return 0;
    return theme->slots[role].attr;
}

static inline int ui_theme_count(void) {
    return UI_THEME_COUNT;
}

