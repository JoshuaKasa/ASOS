#include "asoapi.h"
#include "../lib/string.h"
#include "../lib/stdlib.h"
#include "../ui/theme.h"

#define TERM_MAX_COLS     200
#define TERM_MAX_ROWS     100
#define TERM_MAX_HISTORY  256
#define TERM_INPUT_MAX    128
#define TERM_MAX_FILES    128
#define TERM_NAME_MAX     32
#define TERM_MAX_TOKENS   8

typedef struct {
    char text[TERM_MAX_COLS + 1];
    uint8_t attr;
} term_line_t;

typedef struct {
    ui_theme_id_t id;
    const ui_theme_t* def;
    uint8_t canvas_attr;
    uint8_t text_attr;
    uint8_t muted_attr;
    uint8_t accent_attr;
    uint8_t panel_attr;
    uint8_t panel_text_attr;
    uint8_t status_attr;
    uint8_t status_fill_attr;
    uint8_t prompt_bg_attr;
    uint8_t prompt_label_attr;
    uint8_t prompt_input_attr;
    uint8_t header_bg_attr;
    uint8_t header_text_attr;
    uint8_t cursor_attr;
    uint8_t success_attr;
    uint8_t warning_attr;
    uint8_t command_attr;
    uint8_t info_attr;
} term_theme_t;

static aso_cell_t g_frame[TERM_MAX_COLS * TERM_MAX_ROWS];
static term_line_t g_history[TERM_MAX_HISTORY];
static int g_history_start = 0;
static int g_history_len = 0;
static char g_status_line[TERM_MAX_COLS + 1];
static char g_input[TERM_INPUT_MAX];
static int g_input_len = 0;
static int g_cols = 80;
static int g_rows = 25;
static int g_dirty = 0;
static const char* g_prompt_label = "asos";
static term_theme_t g_theme;

static inline uint8_t attr_fg(uint8_t attr) {
    return attr & 0x0F;
}

static inline uint8_t attr_bg(uint8_t attr) {
    return (attr >> 4) & 0x0F;
}

static inline uint8_t compose_attr(uint8_t fg, uint8_t bg) {
    return UI_ATTR(fg & 0x0F, bg & 0x0F);
}

static char to_upper_char(char c) {
    if (c >= 'a' && c <= 'z')
        return (char)(c - ('a' - 'A'));
    return c;
}

static int str_ieq(const char* a, const char* b) {
    if (!a || !b)
        return 0;
    while (*a && *b) {
        char ca = to_upper_char(*a++);
        char cb = to_upper_char(*b++);
        if (ca != cb)
            return 0;
    }
    return *a == '\0' && *b == '\0';
}

static void mark_dirty(void) {
    g_dirty = 1;
}

static void term_init_dimensions(void) {
    sys_getsize(&g_cols, &g_rows);
    if (g_cols < 40)
        g_cols = 40;
    if (g_rows < 12)
        g_rows = 12;
    if (g_cols > TERM_MAX_COLS)
        g_cols = TERM_MAX_COLS;
    if (g_rows > TERM_MAX_ROWS)
        g_rows = TERM_MAX_ROWS;
}

static void term_apply_theme(ui_theme_id_t id) {
    int count = ui_theme_count();
    if (count <= 0)
        id = UI_THEME_DARK;
    else if ((int)id < 0 || (int)id >= count)
        id = ui_theme_default_id();

    const ui_theme_t* theme = ui_theme_get(id);
    g_theme.id = id;
    g_theme.def = theme;

    uint8_t canvas_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_CANVAS_BG);
    if (!canvas_attr)
        canvas_attr = UI_ATTR(0x0, 0x0);
    g_theme.canvas_attr = canvas_attr;

    uint8_t panel_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_PANEL_BG);
    if (!panel_attr)
        panel_attr = canvas_attr;
    g_theme.panel_attr = panel_attr;

    uint8_t panel_accent_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_PANEL_ACCENT);
    if (!panel_accent_attr)
        panel_accent_attr = panel_attr;

    uint8_t text_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_TEXT_PRIMARY);
    if (!text_attr)
        text_attr = compose_attr(0xF, attr_bg(canvas_attr));

    uint8_t muted_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_TEXT_MUTED);
    if (!muted_attr)
        muted_attr = compose_attr(attr_fg(text_attr), attr_bg(canvas_attr));

    uint8_t accent_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_ACCENT);
    if (!accent_attr)
        accent_attr = text_attr;

    uint8_t success_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_SUCCESS);
    if (!success_attr)
        success_attr = accent_attr;

    uint8_t warning_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_WARNING);
    if (!warning_attr)
        warning_attr = accent_attr;

    uint8_t input_bg_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_INPUT_BG);
    if (!input_bg_attr)
        input_bg_attr = compose_attr(attr_bg(panel_attr), attr_bg(panel_attr));

    uint8_t input_text_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_INPUT_TEXT);
    if (!input_text_attr)
        input_text_attr = compose_attr(attr_fg(text_attr), attr_bg(input_bg_attr));

    uint8_t selection_attr = ui_theme_attr_role(theme, UI_THEME_ROLE_SELECTION);
    if (!selection_attr)
        selection_attr = compose_attr(attr_fg(text_attr), attr_bg(input_bg_attr));

    g_theme.text_attr        = compose_attr(attr_fg(text_attr), attr_bg(canvas_attr));
    g_theme.muted_attr       = compose_attr(attr_fg(muted_attr), attr_bg(canvas_attr));
    g_theme.accent_attr      = compose_attr(attr_fg(accent_attr), attr_bg(canvas_attr));
    g_theme.panel_text_attr  = compose_attr(attr_fg(text_attr), attr_bg(panel_attr));
    g_theme.status_attr      = compose_attr(attr_fg(muted_attr), attr_bg(panel_attr));
    g_theme.status_fill_attr = panel_attr;
    g_theme.prompt_bg_attr   = compose_attr(attr_bg(input_bg_attr), attr_bg(input_bg_attr));
    g_theme.prompt_label_attr = compose_attr(attr_fg(accent_attr), attr_bg(input_bg_attr));
    g_theme.prompt_input_attr = compose_attr(attr_fg(input_text_attr), attr_bg(input_bg_attr));
    g_theme.header_bg_attr   = panel_accent_attr;
    g_theme.header_text_attr = compose_attr(attr_fg(text_attr), attr_bg(panel_accent_attr));
    g_theme.cursor_attr      = selection_attr;
    g_theme.success_attr     = compose_attr(attr_fg(success_attr), attr_bg(canvas_attr));
    g_theme.warning_attr     = compose_attr(attr_fg(warning_attr), attr_bg(canvas_attr));
    g_theme.command_attr     = compose_attr(attr_fg(accent_attr), attr_bg(canvas_attr));
    g_theme.info_attr        = g_theme.muted_attr;
}

static void term_refresh_theme(void) {
    ui_theme_id_t id = (ui_theme_id_t)sys_theme_current();
    term_apply_theme(id);
    mark_dirty();
}

static void history_clear(void) {
    g_history_len = 0;
    g_history_start = 0;
    mark_dirty();
}

static void history_add(const char* text, uint8_t attr) {
    if (!text)
        text = "";
    int len = (int)strlen(text);
    if (len > g_cols - 1)
        len = g_cols - 1;
    if (len < 0)
        len = 0;

    int idx;
    if (g_history_len < TERM_MAX_HISTORY) {
        idx = (g_history_start + g_history_len) % TERM_MAX_HISTORY;
        g_history_len++;
    } else {
        idx = g_history_start;
        g_history_start = (g_history_start + 1) % TERM_MAX_HISTORY;
    }

    for (int i = 0; i < len; ++i)
        g_history[idx].text[i] = text[i];
    g_history[idx].text[len] = '\0';
    g_history[idx].attr = attr;
    mark_dirty();
}

static void set_status(const char* text) {
    if (!text)
        text = "";
    int len = (int)strlen(text);
    if (len > g_cols - 2)
        len = g_cols - 2;
    for (int i = 0; i < len; ++i)
        g_status_line[i] = text[i];
    g_status_line[len] = '\0';
    mark_dirty();
}

static void frame_fill_all(uint8_t attr) {
    int total = g_cols * g_rows;
    if (total > TERM_MAX_COLS * TERM_MAX_ROWS)
        total = TERM_MAX_COLS * TERM_MAX_ROWS;
    for (int i = 0; i < total; ++i) {
        g_frame[i].ch = ' ';
        g_frame[i].attr = attr;
    }
}

static void frame_fill_row(int row, uint8_t attr) {
    if (row < 0 || row >= g_rows)
        return;
    for (int col = 0; col < g_cols; ++col) {
        g_frame[row * g_cols + col].ch = ' ';
        g_frame[row * g_cols + col].attr = attr;
    }
}

static void frame_draw_text(int row, int col, const char* text, uint8_t attr) {
    if (row < 0 || row >= g_rows || col >= g_cols || !text)
        return;
    int idx = row * g_cols + col;
    int total = g_cols * g_rows;
    while (*text && col < g_cols && idx < total) {
        g_frame[idx].ch = *text++;
        g_frame[idx].attr = attr;
        idx++;
        col++;
    }
}

static void render_screen(void) {
    frame_fill_all(g_theme.canvas_attr);

    int header_row = 0;
    frame_fill_row(header_row, g_theme.header_bg_attr);
    char header[128];
    header[0] = '\0';
    strcpy(header, "ASOS Terminal — ");
    strcat(header, ui_theme_name(g_theme.id));
    strcat(header, " theme");
    frame_draw_text(header_row, 2, header, g_theme.header_text_attr);
    if (g_cols > 28)
        frame_draw_text(header_row, g_cols - 26, "help • theme • exit", g_theme.header_text_attr);

    int content_rows = g_rows - 3;
    if (content_rows < 1)
        content_rows = 1;
    int start = (g_history_len > content_rows) ? (g_history_len - content_rows) : 0;
    for (int i = 0; i < content_rows; ++i) {
        int row = 1 + i;
        frame_fill_row(row, g_theme.canvas_attr);
        int line_index = start + i;
        if (line_index < g_history_len) {
            int idx = (g_history_start + line_index) % TERM_MAX_HISTORY;
            frame_draw_text(row, 0, g_history[idx].text, g_history[idx].attr);
        }
    }

    int status_row = g_rows - 2;
    frame_fill_row(status_row, g_theme.status_fill_attr);
    frame_draw_text(status_row, 1, g_status_line, g_theme.status_attr);

    int prompt_row = g_rows - 1;
    frame_fill_row(prompt_row, g_theme.prompt_bg_attr);
    int prompt_col = 0;
    frame_draw_text(prompt_row, prompt_col, g_prompt_label, g_theme.prompt_label_attr);
    prompt_col += (int)strlen(g_prompt_label);
    if (prompt_col < g_cols) {
        frame_draw_text(prompt_row, prompt_col, "> ", g_theme.prompt_label_attr);
        prompt_col += 2;
    }
    frame_draw_text(prompt_row, prompt_col, g_input, g_theme.prompt_input_attr);

    int cursor_col = prompt_col + g_input_len;
    if (cursor_col >= g_cols)
        cursor_col = g_cols - 1;
    if (cursor_col < 0)
        cursor_col = 0;
    if (prompt_row >= 0 && prompt_row < g_rows) {
        aso_cell_t* cell = &g_frame[prompt_row * g_cols + cursor_col];
        if (!cell->ch)
            cell->ch = ' ';
        cell->attr = g_theme.cursor_attr;
    }

    int total = g_cols * g_rows;
    if (total > TERM_MAX_COLS * TERM_MAX_ROWS)
        total = TERM_MAX_COLS * TERM_MAX_ROWS;
    sys_blit(g_frame, total);
    sys_setcursor(cursor_col, prompt_row);
}

static int tokenize(char* line, char* argv[], int max_tokens) {
    int argc = 0;
    char* p = line;
    while (*p && argc < max_tokens) {
        while (*p == ' ')
            ++p;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ')
            ++p;
        if (*p)
            *p++ = '\0';
    }
    return argc;
}

static void join_tokens(char* dest, int dest_size, char* argv[], int start, int argc) {
    int len = 0;
    if (dest_size <= 0)
        return;
    dest[0] = '\0';
    for (int i = start; i < argc && len < dest_size - 1; ++i) {
        if (i > start && len < dest_size - 1)
            dest[len++] = ' ';
        const char* token = argv[i];
        while (*token && len < dest_size - 1)
            dest[len++] = *token++;
    }
    dest[len] = '\0';
}

static uint8_t attr_for_name(const char* name) {
    int len = (int)strlen(name);
    if (len >= 4) {
        const char* ext = name + len - 4;
        if (str_ieq(ext, ".bin"))
            return g_theme.success_attr;
        if (str_ieq(ext, ".txt"))
            return g_theme.accent_attr;
    }
    return g_theme.text_attr;
}

static int find_theme_id(const char* token, ui_theme_id_t* out) {
    if (!token || !token[0])
        return -1;
    int count = ui_theme_count();
    int is_number = 1;
    for (const char* p = token; *p; ++p) {
        if (*p < '0' || *p > '9') {
            is_number = 0;
            break;
        }
    }
    if (is_number) {
        int idx = atoi(token);
        if (idx >= 0 && idx < count) {
            *out = (ui_theme_id_t)idx;
            return 0;
        }
    }
    for (int i = 0; i < count; ++i) {
        if (str_ieq(token, ui_theme_name((ui_theme_id_t)i))) {
            *out = (ui_theme_id_t)i;
            return 0;
        }
    }
    return -1;
}

static void theme_list(void) {
    int count = ui_theme_count();
    if (count <= 0) {
        history_add("No themes available.", g_theme.warning_attr);
        set_status("No themes to list.");
        return;
    }
    history_add("Available themes:", g_theme.info_attr);
    for (int i = 0; i < count; ++i) {
        char line[64];
        line[0] = '\0';
        if (g_theme.id == (ui_theme_id_t)i)
            strcpy(line, "* ");
        else
            strcpy(line, "  ");
        char num[8];
        itoa(i, num, 10);
        strcat(line, num);
        strcat(line, ": ");
        strcat(line, ui_theme_name((ui_theme_id_t)i));
        uint8_t attr = (g_theme.id == (ui_theme_id_t)i) ? g_theme.accent_attr : g_theme.muted_attr;
        history_add(line, attr);
    }
    set_status("Theme list updated.");
}

static void theme_cycle(void) {
    int count = ui_theme_count();
    if (count <= 0) {
        set_status("No alternate themes available.");
        history_add("Theme switch unavailable.", g_theme.warning_attr);
        return;
    }
    ui_theme_id_t next = (g_theme.id + 1) % count;
    if (sys_theme_set(next) == 0) {
        term_apply_theme(next);
        char line[64];
        line[0] = '\0';
        strcpy(line, "Switched to ");
        strcat(line, ui_theme_name(next));
        strcat(line, " theme.");
        history_add(line, g_theme.accent_attr);
        set_status("Theme updated.");
    } else {
        history_add("Unable to switch theme.", g_theme.warning_attr);
        set_status("Theme switch failed.");
    }
}

static void theme_set(const char* token) {
    ui_theme_id_t id;
    if (find_theme_id(token, &id) != 0) {
        history_add("Theme not found.", g_theme.warning_attr);
        set_status("Unknown theme.");
        return;
    }
    if (sys_theme_set(id) == 0) {
        term_apply_theme(id);
        char line[64];
        line[0] = '\0';
        strcpy(line, "Theme set to ");
        strcat(line, ui_theme_name(id));
        strcat(line, ".");
        history_add(line, g_theme.accent_attr);
        set_status("Theme applied.");
    } else {
        history_add("Theme update failed.", g_theme.warning_attr);
        set_status("Theme update failed.");
    }
}

static void command_help(void) {
    history_add("Available commands:", g_theme.info_attr);
    history_add("  help               - Show this message", g_theme.text_attr);
    history_add("  clear              - Clear terminal output", g_theme.text_attr);
    history_add("  list | ls          - List available files", g_theme.text_attr);
    history_add("  cat <file>         - Display a text file", g_theme.text_attr);
    history_add("  run <app>          - Launch an application", g_theme.text_attr);
    history_add("  theme [list|next|set <id|name>]", g_theme.text_attr);
    history_add("  exit               - Return to the kernel shell", g_theme.text_attr);
    set_status("Help ready.");
}

static void command_clear(void) {
    history_clear();
    history_add("History cleared.", g_theme.info_attr);
    set_status("Screen cleared.");
}

static void command_list(void) {
    char raw[TERM_MAX_FILES * TERM_NAME_MAX];
    int count = sys_enumfiles(raw, TERM_MAX_FILES, TERM_NAME_MAX);
    if (count < 0) {
        history_add("Unable to enumerate files.", g_theme.warning_attr);
        set_status("File list failed.");
        return;
    }
    if (count == 0) {
        history_add("[no files found]", g_theme.info_attr);
    } else {
        history_add("Files:", g_theme.info_attr);
        for (int i = 0; i < count; ++i) {
            char* name = raw + i * TERM_NAME_MAX;
            if (!name[0])
                continue;
            char line[TERM_MAX_COLS + 1];
            line[0] = '\0';
            strcpy(line, "  ");
            strcat(line, name);
            history_add(line, attr_for_name(name));
        }
    }
    char status[48];
    status[0] = '\0';
    strcpy(status, "Listed ");
    char num[12];
    itoa(count, num, 10);
    strcat(status, num);
    strcat(status, " item(s).");
    set_status(status);
}

static void command_cat(const char* name) {
    if (!name || !name[0]) {
        history_add("Usage: cat <file>", g_theme.warning_attr);
        set_status("Missing file name.");
        return;
    }
    static char buffer[4096];
    int rc = sys_readfile(name, buffer, (int)sizeof(buffer) - 1);
    if (rc < 0) {
        history_add("Unable to read file.", g_theme.warning_attr);
        set_status("Read failed.");
        return;
    }
    buffer[rc] = '\0';
    if (rc == 0) {
        history_add("[empty file]", g_theme.info_attr);
    } else {
        char* p = buffer;
        while (*p) {
            char line[TERM_MAX_COLS + 1];
            int len = 0;
            while (*p && *p != '\n' && len < g_cols - 1)
                line[len++] = *p++;
            if (*p == '\n')
                p++;
            line[len] = '\0';
            history_add(line, g_theme.text_attr);
        }
        if (rc >= (int)sizeof(buffer) - 1)
            history_add("[output truncated]", g_theme.warning_attr);
    }
    set_status("File displayed.");
}

static void command_run(const char* target) {
    if (!target || !target[0]) {
        history_add("Usage: run <app>", g_theme.warning_attr);
        set_status("Missing application name.");
        return;
    }
    char line[TERM_MAX_COLS + 1];
    line[0] = '\0';
    strcpy(line, "Launching ");
    strcat(line, target);
    strcat(line, "...");
    history_add(line, g_theme.command_attr);
    set_status("Running application.");
    render_screen();
    g_dirty = 0;
    sys_exec(target);
    history_add("Application returned control.", g_theme.info_attr);
    set_status("Application finished.");
    term_refresh_theme();
}

static void command_exit(void) {
    history_add("Exiting terminal...", g_theme.info_attr);
    set_status("Goodbye.");
    render_screen();
    g_dirty = 0;
    sys_exit();
}

static void command_theme(int argc, char* argv[]) {
    if (argc == 1) {
        char line[64];
        line[0] = '\0';
        strcpy(line, "Current theme: ");
        strcat(line, ui_theme_name(g_theme.id));
        char idbuf[8];
        itoa(g_theme.id, idbuf, 10);
        strcat(line, " (#");
        strcat(line, idbuf);
        strcat(line, ")");
        history_add(line, g_theme.info_attr);
        set_status("Theme info ready.");
        return;
    }
    if (str_ieq(argv[1], "list")) {
        theme_list();
        return;
    }
    if (str_ieq(argv[1], "next")) {
        theme_cycle();
        return;
    }
    if (str_ieq(argv[1], "set") && argc >= 3) {
        theme_set(argv[2]);
        return;
    }
    history_add("Usage: theme [list|next|set <id|name>]", g_theme.warning_attr);
    set_status("Theme usage shown.");
}

static void handle_command(const char* raw_line) {
    char buffer[TERM_INPUT_MAX];
    buffer[0] = '\0';
    if (raw_line)
        strcpy(buffer, raw_line);
    char* line = buffer;
    while (*line == ' ')
        ++line;
    if (!*line) {
        set_status("Ready.");
        return;
    }
    char* argv[TERM_MAX_TOKENS];
    int argc = tokenize(line, argv, TERM_MAX_TOKENS);
    if (argc == 0) {
        set_status("Ready.");
        return;
    }
    if (str_ieq(argv[0], "help")) {
        command_help();
    } else if (str_ieq(argv[0], "clear") || str_ieq(argv[0], "cls")) {
        command_clear();
    } else if (str_ieq(argv[0], "list") || str_ieq(argv[0], "ls")) {
        command_list();
    } else if (str_ieq(argv[0], "cat")) {
        char joined[TERM_INPUT_MAX];
        join_tokens(joined, sizeof(joined), argv, 1, argc);
        command_cat(joined);
    } else if (str_ieq(argv[0], "run") || str_ieq(argv[0], "open")) {
        char joined[TERM_INPUT_MAX];
        join_tokens(joined, sizeof(joined), argv, 1, argc);
        command_run(joined);
    } else if (str_ieq(argv[0], "desktop")) {
        command_run("desktop.bin");
    } else if (str_ieq(argv[0], "theme")) {
        command_theme(argc, argv);
    } else if (str_ieq(argv[0], "exit") || str_ieq(argv[0], "quit")) {
        command_exit();
    } else {
        char line_out[TERM_MAX_COLS + 1];
        line_out[0] = '\0';
        strcpy(line_out, "Unknown command: ");
        strcat(line_out, argv[0]);
        history_add(line_out, g_theme.warning_attr);
        set_status("Type 'help' for options.");
    }
}

void main(void) {
    sys_clear();
    term_init_dimensions();
    term_refresh_theme();
    history_clear();
    g_input[0] = '\0';
    g_input_len = 0;
    history_add("Welcome to the ASOS terminal.", g_theme.info_attr);
    history_add("Type 'help' to explore available commands.", g_theme.info_attr);
    set_status("Ready.");

    while (1) {
        ui_theme_id_t kernel_theme = (ui_theme_id_t)sys_theme_current();
        if (kernel_theme != g_theme.id) {
            term_apply_theme(kernel_theme);
            char line[64];
            line[0] = '\0';
            strcpy(line, "Theme synced to ");
            strcat(line, ui_theme_name(kernel_theme));
            strcat(line, ".");
            history_add(line, g_theme.info_attr);
            set_status("Theme synchronized.");
        }

        if (g_dirty) {
            render_screen();
            g_dirty = 0;
        }

        unsigned int key = sys_trygetchar();
        if (!key) {
            asm volatile("hlt");
            continue;
        }

        char c = (char)key;
        if (c == '\n') {
            g_input[g_input_len] = '\0';
            if (g_input_len > 0) {
                char prompt_line[TERM_MAX_COLS + 1];
                prompt_line[0] = '\0';
                strcpy(prompt_line, g_prompt_label);
                strcat(prompt_line, "> ");
                strcat(prompt_line, g_input);
                history_add(prompt_line, g_theme.command_attr);
            }
            char command[TERM_INPUT_MAX];
            command[0] = '\0';
            strcpy(command, g_input);
            g_input[0] = '\0';
            g_input_len = 0;
            handle_command(command);
        } else if (c == '\b' || c == 0x7F) {
            if (g_input_len > 0) {
                g_input_len--;
                g_input[g_input_len] = '\0';
                mark_dirty();
            }
        } else if (c == 3) { // Ctrl+C
            g_input[0] = '\0';
            g_input_len = 0;
            set_status("Input cleared.");
        } else if (c == 12) { // Ctrl+L
            command_clear();
        } else if (c >= 32 && c < 127) {
            if (g_input_len < TERM_INPUT_MAX - 1) {
                g_input[g_input_len++] = c;
                g_input[g_input_len] = '\0';
                mark_dirty();
            }
        }
    }
}

