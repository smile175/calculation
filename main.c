#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define MAX_EXPR 512

#define ID_DISPLAY_MAIN 100
#define ID_DISPLAY_SMALL 101

#define ID_BTN_0 200
#define ID_BTN_1 201
#define ID_BTN_2 202
#define ID_BTN_3 203
#define ID_BTN_4 204
#define ID_BTN_5 205
#define ID_BTN_6 206
#define ID_BTN_7 207
#define ID_BTN_8 208
#define ID_BTN_9 209
#define ID_BTN_DOT 210
#define ID_BTN_ADD 211
#define ID_BTN_SUB 212
#define ID_BTN_MUL 213
#define ID_BTN_DIV 214
#define ID_BTN_EQ 215
#define ID_BTN_DEL 216
#define ID_BTN_AC 217

static HWND g_display_main = NULL;
static HWND g_display_small = NULL;

static char g_expr[MAX_EXPR] = "";
static int g_just_evaluated = 0;

static int is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/';
}

static void set_text(HWND h, const char *text) {
    SetWindowTextA(h, text);
}

static void update_main_display(void) {
    if (g_expr[0] == '\0') {
        set_text(g_display_main, "0");
    } else {
        set_text(g_display_main, g_expr);
    }
}

static int current_number_has_dot(void) {
    int i = (int)strlen(g_expr) - 1;
    while (i >= 0 && !is_operator(g_expr[i])) {
        if (g_expr[i] == '.') return 1;
        i--;
    }
    return 0;
}

static void append_char(char c) {
    size_t len = strlen(g_expr);
    if (len + 1 >= MAX_EXPR) return;
    g_expr[len] = c;
    g_expr[len + 1] = '\0';
}

static void replace_last_char(char c) {
    size_t len = strlen(g_expr);
    if (len == 0) return;
    g_expr[len - 1] = c;
}

static void clear_all(void) {
    g_expr[0] = '\0';
    set_text(g_display_small, "");
    update_main_display();
    g_just_evaluated = 0;
}

// Expression evaluation (shunting-yard for + - * /)
// Supports unary minus when it appears at start or after operator.

typedef struct {
    double *data;
    int top;
    int cap;
} DStack;

typedef struct {
    char *data;
    int top;
    int cap;
} CStack;

static void dstack_init(DStack *s, int cap) {
    s->data = (double*)malloc(sizeof(double) * cap);
    s->top = 0;
    s->cap = cap;
}

static void cstack_init(CStack *s, int cap) {
    s->data = (char*)malloc(sizeof(char) * cap);
    s->top = 0;
    s->cap = cap;
}

static void dstack_free(DStack *s) { free(s->data); }
static void cstack_free(CStack *s) { free(s->data); }

static void dpush(DStack *s, double v) { s->data[s->top++] = v; }
static double dpop(DStack *s) { return s->data[--s->top]; }
static void cpush(CStack *s, char v) { s->data[s->top++] = v; }
static char cpop(CStack *s) { return s->data[--s->top]; }
static char cpeek(CStack *s) { return s->data[s->top - 1]; }

static int precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

static int apply_op(DStack *values, char op) {
    if (values->top < 2) return 0;
    double b = dpop(values);
    double a = dpop(values);
    double r = 0.0;
    switch (op) {
        case '+': r = a + b; break;
        case '-': r = a - b; break;
        case '*': r = a * b; break;
        case '/':
            if (b == 0.0) return 0;
            r = a / b;
            break;
        default: return 0;
    }
    dpush(values, r);
    return 1;
}

static int evaluate_expression(const char *expr, double *out) {
    DStack values;
    CStack ops;
    dstack_init(&values, 256);
    cstack_init(&ops, 256);

    int i = 0;
    int len = (int)strlen(expr);
    int expect_number = 1;

    while (i < len) {
        if (isspace((unsigned char)expr[i])) { i++; continue; }

        if (expect_number) {
            // parse number with optional unary minus
            int start = i;
            if (expr[i] == '-') {
                i++;
            }
            int has_digit = 0;
            while (i < len && (isdigit((unsigned char)expr[i]) || expr[i] == '.')) {
                if (isdigit((unsigned char)expr[i])) has_digit = 1;
                i++;
            }
            if (!has_digit) {
                dstack_free(&values); cstack_free(&ops);
                return 0;
            }
            char buf[64];
            int n = i - start;
            if (n >= (int)sizeof(buf)) { dstack_free(&values); cstack_free(&ops); return 0; }
            memcpy(buf, expr + start, n);
            buf[n] = '\0';
            double val = strtod(buf, NULL);
            dpush(&values, val);
            expect_number = 0;
            continue;
        }

        if (is_operator(expr[i])) {
            char op = expr[i];
            while (ops.top > 0 && precedence(cpeek(&ops)) >= precedence(op)) {
                if (!apply_op(&values, cpop(&ops))) {
                    dstack_free(&values); cstack_free(&ops);
                    return 0;
                }
            }
            cpush(&ops, op);
            i++;
            expect_number = 1;
            continue;
        }

        // invalid character
        dstack_free(&values); cstack_free(&ops);
        return 0;
    }

    while (ops.top > 0) {
        if (!apply_op(&values, cpop(&ops))) {
            dstack_free(&values); cstack_free(&ops);
            return 0;
        }
    }

    if (values.top != 1) {
        dstack_free(&values); cstack_free(&ops);
        return 0;
    }

    *out = dpop(&values);
    dstack_free(&values); cstack_free(&ops);
    return 1;
}

static void on_digit(char d) {
    if (g_just_evaluated) {
        g_expr[0] = '\0';
        set_text(g_display_small, "");
        g_just_evaluated = 0;
    }
    if (g_expr[0] == '0' && g_expr[1] == '\0') {
        if (d == '0') {
            update_main_display();
            return;
        }
        g_expr[0] = d;
        g_expr[1] = '\0';
    } else {
        append_char(d);
    }
    update_main_display();
}

static void on_dot(void) {
    if (g_just_evaluated) {
        g_expr[0] = '\0';
        set_text(g_display_small, "");
        g_just_evaluated = 0;
    }
    if (!current_number_has_dot()) {
        if (g_expr[0] == '\0' || is_operator(g_expr[strlen(g_expr) - 1])) {
            append_char('0');
        }
        append_char('.');
        update_main_display();
    }
}

static void on_operator(char op) {
    if (g_expr[0] == '\0') {
        if (op == '-') {
            append_char('-');
            update_main_display();
        }
        return;
    }
    size_t len = strlen(g_expr);
    char last = g_expr[len - 1];
    if (is_operator(last)) {
        replace_last_char(op);
    } else {
        append_char(op);
    }
    update_main_display();
    g_just_evaluated = 0;
}

static void on_delete(void) {
    size_t len = strlen(g_expr);
    if (len == 0) return;
    g_expr[len - 1] = '\0';
    update_main_display();
}

static void format_double(double v, char *out, size_t out_cap) {
    // Remove trailing zeros for a cleaner display.
    char buf[64];
    snprintf(buf, sizeof(buf), "%.12g", v);
    strncpy(out, buf, out_cap - 1);
    out[out_cap - 1] = '\0';
}

static void on_equals(void) {
    if (g_expr[0] == '\0') return;
    double result = 0.0;
    if (!evaluate_expression(g_expr, &result)) {
        set_text(g_display_small, g_expr);
        set_text(g_display_main, "Error");
        g_expr[0] = '\0';
        g_just_evaluated = 1;
        return;
    }
    char resbuf[64];
    format_double(result, resbuf, sizeof(resbuf));
    set_text(g_display_small, g_expr);
    set_text(g_display_main, resbuf);
    strncpy(g_expr, resbuf, sizeof(g_expr) - 1);
    g_expr[sizeof(g_expr) - 1] = '\0';
    g_just_evaluated = 1;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hFontMain = CreateFontA(36, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                          ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                                          CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                          DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            HFONT hFontSmall = CreateFontA(16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                           ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                                           CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                           DEFAULT_PITCH | FF_SWISS, "Segoe UI");

            g_display_small = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                            10, 10, 320, 24, hwnd, (HMENU)ID_DISPLAY_SMALL,
                                            NULL, NULL);
            g_display_main = CreateWindowA("STATIC", "0", WS_CHILD | WS_VISIBLE | SS_RIGHT,
                                           10, 38, 320, 48, hwnd, (HMENU)ID_DISPLAY_MAIN,
                                           NULL, NULL);

            SendMessageA(g_display_small, WM_SETFONT, (WPARAM)hFontSmall, TRUE);
            SendMessageA(g_display_main, WM_SETFONT, (WPARAM)hFontMain, TRUE);

            int x0 = 10, y0 = 100;
            int w = 75, h = 60, gap = 5;

            // Row 1
            CreateWindowA("BUTTON", "AC", WS_CHILD | WS_VISIBLE, x0 + 0*(w+gap), y0, w, h, hwnd, (HMENU)ID_BTN_AC, NULL, NULL);
            CreateWindowA("BUTTON", "Del", WS_CHILD | WS_VISIBLE, x0 + 1*(w+gap), y0, w, h, hwnd, (HMENU)ID_BTN_DEL, NULL, NULL);
            CreateWindowA("BUTTON", "/", WS_CHILD | WS_VISIBLE, x0 + 2*(w+gap), y0, w, h, hwnd, (HMENU)ID_BTN_DIV, NULL, NULL);
            CreateWindowA("BUTTON", "*", WS_CHILD | WS_VISIBLE, x0 + 3*(w+gap), y0, w, h, hwnd, (HMENU)ID_BTN_MUL, NULL, NULL);

            // Row 2
            CreateWindowA("BUTTON", "7", WS_CHILD | WS_VISIBLE, x0 + 0*(w+gap), y0 + 1*(h+gap), w, h, hwnd, (HMENU)ID_BTN_7, NULL, NULL);
            CreateWindowA("BUTTON", "8", WS_CHILD | WS_VISIBLE, x0 + 1*(w+gap), y0 + 1*(h+gap), w, h, hwnd, (HMENU)ID_BTN_8, NULL, NULL);
            CreateWindowA("BUTTON", "9", WS_CHILD | WS_VISIBLE, x0 + 2*(w+gap), y0 + 1*(h+gap), w, h, hwnd, (HMENU)ID_BTN_9, NULL, NULL);
            CreateWindowA("BUTTON", "-", WS_CHILD | WS_VISIBLE, x0 + 3*(w+gap), y0 + 1*(h+gap), w, h, hwnd, (HMENU)ID_BTN_SUB, NULL, NULL);

            // Row 3
            CreateWindowA("BUTTON", "4", WS_CHILD | WS_VISIBLE, x0 + 0*(w+gap), y0 + 2*(h+gap), w, h, hwnd, (HMENU)ID_BTN_4, NULL, NULL);
            CreateWindowA("BUTTON", "5", WS_CHILD | WS_VISIBLE, x0 + 1*(w+gap), y0 + 2*(h+gap), w, h, hwnd, (HMENU)ID_BTN_5, NULL, NULL);
            CreateWindowA("BUTTON", "6", WS_CHILD | WS_VISIBLE, x0 + 2*(w+gap), y0 + 2*(h+gap), w, h, hwnd, (HMENU)ID_BTN_6, NULL, NULL);
            CreateWindowA("BUTTON", "+", WS_CHILD | WS_VISIBLE, x0 + 3*(w+gap), y0 + 2*(h+gap), w, h, hwnd, (HMENU)ID_BTN_ADD, NULL, NULL);

            // Row 4
            CreateWindowA("BUTTON", "1", WS_CHILD | WS_VISIBLE, x0 + 0*(w+gap), y0 + 3*(h+gap), w, h, hwnd, (HMENU)ID_BTN_1, NULL, NULL);
            CreateWindowA("BUTTON", "2", WS_CHILD | WS_VISIBLE, x0 + 1*(w+gap), y0 + 3*(h+gap), w, h, hwnd, (HMENU)ID_BTN_2, NULL, NULL);
            CreateWindowA("BUTTON", "3", WS_CHILD | WS_VISIBLE, x0 + 2*(w+gap), y0 + 3*(h+gap), w, h, hwnd, (HMENU)ID_BTN_3, NULL, NULL);
            CreateWindowA("BUTTON", "=", WS_CHILD | WS_VISIBLE, x0 + 3*(w+gap), y0 + 3*(h+gap), w, h, hwnd, (HMENU)ID_BTN_EQ, NULL, NULL);

            // Row 5 (0 spans two columns)
            CreateWindowA("BUTTON", "0", WS_CHILD | WS_VISIBLE, x0 + 0*(w+gap), y0 + 4*(h+gap), w*2 + gap, h, hwnd, (HMENU)ID_BTN_0, NULL, NULL);
            CreateWindowA("BUTTON", ".", WS_CHILD | WS_VISIBLE, x0 + 2*(w+gap), y0 + 4*(h+gap), w, h, hwnd, (HMENU)ID_BTN_DOT, NULL, NULL);

            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            switch (id) {
                case ID_BTN_0: on_digit('0'); break;
                case ID_BTN_1: on_digit('1'); break;
                case ID_BTN_2: on_digit('2'); break;
                case ID_BTN_3: on_digit('3'); break;
                case ID_BTN_4: on_digit('4'); break;
                case ID_BTN_5: on_digit('5'); break;
                case ID_BTN_6: on_digit('6'); break;
                case ID_BTN_7: on_digit('7'); break;
                case ID_BTN_8: on_digit('8'); break;
                case ID_BTN_9: on_digit('9'); break;
                case ID_BTN_DOT: on_dot(); break;
                case ID_BTN_ADD: on_operator('+'); break;
                case ID_BTN_SUB: on_operator('-'); break;
                case ID_BTN_MUL: on_operator('*'); break;
                case ID_BTN_DIV: on_operator('/'); break;
                case ID_BTN_DEL: on_delete(); break;
                case ID_BTN_AC: clear_all(); break;
                case ID_BTN_EQ: on_equals(); break;
                default: break;
            }
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev;
    (void)lpCmd;
    const char *CLASS_NAME = "CalcWindowClass";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExA(
        0, CLASS_NAME, "Calculator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 520,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return 0;
}
