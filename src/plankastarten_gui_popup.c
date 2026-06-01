#include "plankastarten_gui_popup.h"
#include "plankastarten_gui_ids.h"
#include "plankastarten_gui_theme.h"

#include <stdio.h>
#include <string.h>

#define PST_POPUP_CLASS "PlankaStartenPopup"
#define PST_POPUP_HEADER_CLASS "PlankaStartenPopupHeader"
#define PST_POPUP_MENU_ITEM_CLASS "PlankaStartenPopupMenuItem"
#define PST_POPUP_TITLE 9001
#define PST_POPUP_BODY 9002
#define PST_POPUP_OK 9003
#define PST_POPUP_CLOSE 9004
#define PST_POPUP_INPUT 9005
#define PST_POPUP_APPLY 9006
#define PST_POPUP_SECONDARY 9007

#define PST_MENU_ITEM_CHECKED 0x01
#define PST_MENU_ITEM_HOT 0x02

static int pst_popup_registered;
static int pst_header_registered;
static int pst_menu_item_registered;

static PST_POPUP *pst_from_hwnd(HWND hwnd)
{
    return (PST_POPUP *)(INT_PTR)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
}

static void pst_popup_set_font(HWND hwnd, HFONT font)
{
    if (hwnd != 0 && font != 0) {
        SendMessageA(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    }
}

static LRESULT CALLBACK pst_popup_header_wndproc(HWND hwnd, UINT msg,
    WPARAM wp, LPARAM lp)
{
    HFONT font;
    PAINTSTRUCT ps;
    HDC dc;
    RECT rc;
    HGDIOBJ old_font;
    char text[256];
    HBRUSH brush;

    switch (msg) {
    case WM_SETFONT:
        SetWindowLongPtrA(hwnd, 0, (LONG_PTR)wp);
        if (LOWORD(lp)) {
            InvalidateRect(hwnd, 0, TRUE);
        }
        return 0;
    case WM_GETFONT:
        return GetWindowLongPtrA(hwnd, 0);
    case WM_LBUTTONDOWN:
        ReleaseCapture();
        SendMessageA(GetParent(hwnd), WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
        return 0;
    case WM_PAINT:
        dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        brush = CreateSolidBrush(PST_COLOR_ORANGE);
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        GetWindowTextA(hwnd, text, sizeof(text));
        font = (HFONT)GetWindowLongPtrA(hwnd, 0);
        old_font = font != 0 ? SelectObject(dc, font) : 0;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        rc.left += 10;
        rc.right -= 34;
        DrawTextA(dc, text, -1, &rc,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (old_font != 0) {
            SelectObject(dc, old_font);
        }
        EndPaint(hwnd, &ps);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK pst_menu_item_wndproc(HWND hwnd, UINT msg,
    WPARAM wp, LPARAM lp)
{
    PAINTSTRUCT ps;
    HDC dc;
    RECT rc;
    POINT pt;
    int flags;
    HFONT font;
    HGDIOBJ old_font;
    HBRUSH brush;
    char text[256];
    TRACKMOUSEEVENT tme;

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrA(hwnd, 0,
            (LONG_PTR)((CREATESTRUCTA *)lp)->lpCreateParams);
        return 0;
    case WM_SETFONT:
        SetWindowLongPtrA(hwnd, sizeof(LONG_PTR), (LONG_PTR)wp);
        if (LOWORD(lp)) {
            InvalidateRect(hwnd, 0, TRUE);
        }
        return 0;
    case WM_GETFONT:
        return GetWindowLongPtrA(hwnd, sizeof(LONG_PTR));
    case WM_MOUSEMOVE:
        flags = (int)GetWindowLongPtrA(hwnd, 0);
        if ((flags & PST_MENU_ITEM_HOT) == 0) {
            flags |= PST_MENU_ITEM_HOT;
            SetWindowLongPtrA(hwnd, 0, (LONG_PTR)flags);
            memset(&tme, 0, sizeof(tme));
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, 0, TRUE);
        }
        return 0;
    case WM_MOUSELEAVE:
        flags = (int)GetWindowLongPtrA(hwnd, 0);
        flags &= ~PST_MENU_ITEM_HOT;
        SetWindowLongPtrA(hwnd, 0, (LONG_PTR)flags);
        InvalidateRect(hwnd, 0, TRUE);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP:
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        GetClientRect(hwnd, &rc);
        pt.x = LOWORD(lp);
        pt.y = HIWORD(lp);
        if (PtInRect(&rc, pt)) {
            SendMessageA(GetParent(hwnd), WM_COMMAND,
                MAKEWPARAM(GetDlgCtrlID(hwnd), 0), (LPARAM)hwnd);
        }
        return 0;
    case WM_CAPTURECHANGED:
        flags = (int)GetWindowLongPtrA(hwnd, 0);
        flags &= ~PST_MENU_ITEM_HOT;
        SetWindowLongPtrA(hwnd, 0, (LONG_PTR)flags);
        InvalidateRect(hwnd, 0, TRUE);
        return 0;
    case WM_PAINT:
        dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        flags = (int)GetWindowLongPtrA(hwnd, 0);
        brush = CreateSolidBrush((flags & PST_MENU_ITEM_HOT) != 0
            ? PST_COLOR_ORANGE : RGB(255, 255, 255));
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        GetWindowTextA(hwnd, text, sizeof(text));
        font = (HFONT)GetWindowLongPtrA(hwnd, sizeof(LONG_PTR));
        old_font = font != 0 ? SelectObject(dc, font) : 0;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, (flags & PST_MENU_ITEM_HOT) != 0
            ? RGB(255, 255, 255) : PST_COLOR_TEXT);
        rc.left += 8;
        rc.right -= 8;
        DrawTextA(dc, text, -1, &rc,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (old_font != 0) {
            SelectObject(dc, old_font);
        }
        EndPaint(hwnd, &ps);
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pst_popup_forward_and_close(HWND hwnd, int id)
{
    PST_POPUP *popup;
    HWND owner;

    popup = pst_from_hwnd(hwnd);
    owner = popup != 0 ? popup->owner : 0;
    if (popup != 0) {
        HWND input;

        popup->last_text[0] = '\0';
        input = GetDlgItem(hwnd, PST_POPUP_INPUT);
        if (input != 0) {
            GetWindowTextA(input, popup->last_text,
                sizeof(popup->last_text));
        }
    }
    DestroyWindow(hwnd);
    if (popup != 0) {
        popup->hwnd = 0;
    }
    if (owner != 0 && id > 0) {
        SendMessageA(owner, WM_COMMAND, MAKEWPARAM(id, 0), 0);
    }
}

static LRESULT CALLBACK pst_popup_wndproc(HWND hwnd, UINT msg,
    WPARAM wp, LPARAM lp)
{
    PST_POPUP *popup;
    int id;

    switch (msg) {
    case WM_CREATE:
        popup = (PST_POPUP *)((CREATESTRUCTA *)lp)->lpCreateParams;
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)popup);
        return 0;
    case WM_CTLCOLORSTATIC:
        popup = pst_from_hwnd(hwnd);
        id = GetDlgCtrlID((HWND)lp);
        if (popup == 0) {
            break;
        }
        if (id == PST_POPUP_TITLE) {
            SetTextColor((HDC)wp, RGB(255, 255, 255));
            SetBkColor((HDC)wp, PST_COLOR_ORANGE);
            return (LRESULT)popup->orange_brush;
        }
        SetTextColor((HDC)wp, PST_COLOR_TEXT);
        SetBkColor((HDC)wp, RGB(255, 255, 255));
        return (LRESULT)popup->white_brush;
    case WM_COMMAND:
        id = LOWORD(wp);
        if (id == PST_POPUP_OK || id == PST_POPUP_CLOSE) {
            pst_popup_forward_and_close(hwnd, 0);
            return 0;
        }
        if (id == PST_POPUP_APPLY) {
            pst_popup_forward_and_close(hwnd, IDM_POPUP_SEARCH_APPLY);
            return 0;
        }
        if (id == PST_POPUP_SECONDARY) {
            pst_popup_forward_and_close(hwnd, IDM_POPUP_SEARCH_FOCUS_PROC);
            return 0;
        }
        pst_popup_forward_and_close(hwnd, id);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            pst_popup_forward_and_close(hwnd, 0);
            return 0;
        }
        break;
    case WM_DESTROY:
        popup = pst_from_hwnd(hwnd);
        if (popup != 0 && popup->hwnd == hwnd) {
            popup->hwnd = 0;
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pst_popup_register(void)
{
    WNDCLASSA wc;

    if (!pst_popup_registered) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = pst_popup_wndproc;
        wc.hInstance = GetModuleHandleA(0);
        wc.lpszClassName = PST_POPUP_CLASS;
        wc.hCursor = LoadCursorA(0, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassA(&wc);
        pst_popup_registered = 1;
    }
    if (!pst_header_registered) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = pst_popup_header_wndproc;
        wc.hInstance = GetModuleHandleA(0);
        wc.lpszClassName = PST_POPUP_HEADER_CLASS;
        wc.hCursor = LoadCursorA(0, IDC_SIZEALL);
        wc.cbWndExtra = sizeof(LONG_PTR);
        RegisterClassA(&wc);
        pst_header_registered = 1;
    }
    if (!pst_menu_item_registered) {
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = pst_menu_item_wndproc;
        wc.hInstance = GetModuleHandleA(0);
        wc.lpszClassName = PST_POPUP_MENU_ITEM_CLASS;
        wc.hCursor = LoadCursorA(0, IDC_ARROW);
        wc.cbWndExtra = sizeof(LONG_PTR) * 2;
        RegisterClassA(&wc);
        pst_menu_item_registered = 1;
    }
}

static void pst_popup_center(HWND owner, int w, int h, int *x, int *y)
{
    RECT rc;
    RECT work;

    GetWindowRect(owner, &rc);
    *x = rc.left + ((rc.right - rc.left) - w) / 2;
    *y = rc.top + ((rc.bottom - rc.top) - h) / 2;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    if (*x < work.left) *x = work.left;
    if (*y < work.top) *y = work.top;
    if (*x + w > work.right) *x = work.right - w;
    if (*y + h > work.bottom) *y = work.bottom - h;
}

static void pst_popup_place_menu(HWND anchor, int w, int h, int *x, int *y)
{
    RECT rc;
    RECT work;

    GetWindowRect(anchor, &rc);
    *x = rc.left;
    *y = rc.bottom + 1;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    if (*x + w > work.right) *x = work.right - w;
    if (*y + h > work.bottom) *y = rc.top - h - 1;
    if (*x < work.left) *x = work.left;
    if (*y < work.top) *y = work.top;
}

void pst_popup_init(PST_POPUP *popup)
{
    if (popup == 0) {
        return;
    }
    memset(popup, 0, sizeof(*popup));
    popup->bg_brush = CreateSolidBrush(PST_COLOR_BG);
    popup->orange_brush = CreateSolidBrush(PST_COLOR_ORANGE);
    popup->dark_brush = CreateSolidBrush(PST_COLOR_DARK);
    popup->white_brush = CreateSolidBrush(RGB(255, 255, 255));
}

void pst_popup_destroy(PST_POPUP *popup)
{
    if (popup == 0) {
        return;
    }
    pst_popup_close(popup);
    if (popup->bg_brush != 0) DeleteObject(popup->bg_brush);
    if (popup->orange_brush != 0) DeleteObject(popup->orange_brush);
    if (popup->dark_brush != 0) DeleteObject(popup->dark_brush);
    if (popup->white_brush != 0) DeleteObject(popup->white_brush);
    memset(popup, 0, sizeof(*popup));
}

void pst_popup_close(PST_POPUP *popup)
{
    if (popup != 0 && popup->hwnd != 0) {
        DestroyWindow(popup->hwnd);
        popup->hwnd = 0;
    }
}

void pst_popup_show_message(PST_POPUP *popup, HWND owner,
    const char *title, const char *body, HFONT title_font, HFONT ui_font)
{
    HWND hwnd;
    HWND header;
    HWND close_button;
    HWND body_text;
    HWND ok_button;
    int x;
    int y;
    int w;
    int h;

    if (popup == 0 || owner == 0) {
        return;
    }
    pst_popup_register();
    pst_popup_close(popup);
    popup->owner = owner;
    popup->title_font = title_font;
    popup->ui_font = ui_font;
    w = 470;
    h = 172;
    pst_popup_center(owner, w, h, &x, &y);
    hwnd = CreateWindowExA(WS_EX_TOOLWINDOW, PST_POPUP_CLASS, "",
        WS_POPUP | WS_BORDER | WS_VISIBLE,
        x, y, w, h, owner, 0, GetModuleHandleA(0), popup);
    if (hwnd == 0) {
        return;
    }
    popup->hwnd = hwnd;
    header = CreateWindowExA(0, PST_POPUP_HEADER_CLASS,
        title != 0 ? title : "",
        WS_CHILD | WS_VISIBLE,
        0, 0, w, 28, hwnd, (HMENU)(INT_PTR)PST_POPUP_TITLE,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(header, title_font);
    close_button = CreateWindowExA(0, "BUTTON", "X",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 34, 4, 26, 20, hwnd, (HMENU)(INT_PTR)PST_POPUP_CLOSE,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(close_button, ui_font);
    body_text = CreateWindowExA(0, "STATIC", body != 0 ? body : "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        18, 44, w - 36, 78, hwnd, (HMENU)(INT_PTR)PST_POPUP_BODY,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(body_text, ui_font);
    ok_button = CreateWindowExA(0, "BUTTON", "OK",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 92, h - 38, 74, 24, hwnd, (HMENU)(INT_PTR)PST_POPUP_OK,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(ok_button, ui_font);
    SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
    SetFocus(ok_button);
}

void pst_popup_show_search(PST_POPUP *popup, HWND owner, const char *title,
    const char *label, const char *initial, const char *find_text,
    const char *list_text, HFONT title_font, HFONT ui_font)
{
    HWND hwnd;
    HWND header;
    HWND close_button;
    HWND body_label;
    HWND input;
    HWND apply_button;
    HWND list_button;
    int x;
    int y;
    int w;
    int h;

    if (popup == 0 || owner == 0) {
        return;
    }
    pst_popup_register();
    pst_popup_close(popup);
    popup->owner = owner;
    popup->title_font = title_font;
    popup->ui_font = ui_font;
    popup->last_text[0] = '\0';
    w = 420;
    h = 146;
    pst_popup_center(owner, w, h, &x, &y);
    hwnd = CreateWindowExA(WS_EX_TOOLWINDOW, PST_POPUP_CLASS, "",
        WS_POPUP | WS_BORDER | WS_VISIBLE,
        x, y, w, h, owner, 0, GetModuleHandleA(0), popup);
    if (hwnd == 0) {
        return;
    }
    popup->hwnd = hwnd;
    header = CreateWindowExA(0, PST_POPUP_HEADER_CLASS,
        title != 0 ? title : "",
        WS_CHILD | WS_VISIBLE,
        0, 0, w, 28, hwnd, (HMENU)(INT_PTR)PST_POPUP_TITLE,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(header, title_font);
    close_button = CreateWindowExA(0, "BUTTON", "X",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 34, 4, 26, 20, hwnd, (HMENU)(INT_PTR)PST_POPUP_CLOSE,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(close_button, ui_font);
    body_label = CreateWindowExA(0, "STATIC", label != 0 ? label : "",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        18, 42, w - 36, 18, hwnd, (HMENU)(INT_PTR)PST_POPUP_BODY,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(body_label, ui_font);
    input = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT",
        initial != 0 ? initial : "",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        18, 64, w - 36, 24, hwnd, (HMENU)(INT_PTR)PST_POPUP_INPUT,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(input, ui_font);
    list_button = CreateWindowExA(0, "BUTTON",
        list_text != 0 ? list_text : "List",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        18, h - 38, 130, 24, hwnd, (HMENU)(INT_PTR)PST_POPUP_SECONDARY,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(list_button, ui_font);
    apply_button = CreateWindowExA(0, "BUTTON",
        find_text != 0 ? find_text : "Find",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        w - 96, h - 38, 78, 24, hwnd, (HMENU)(INT_PTR)PST_POPUP_APPLY,
        GetModuleHandleA(0), 0);
    pst_popup_set_font(apply_button, ui_font);
    SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
    SetFocus(input);
    SendMessageA(input, EM_SETSEL, 0, -1);
}

int pst_popup_get_text(PST_POPUP *popup, char *out, unsigned out_size)
{
    if (popup == 0 || out == 0 || out_size == 0) {
        return 0;
    }
    strncpy(out, popup->last_text, out_size - 1);
    out[out_size - 1] = '\0';
    return out[0] != '\0';
}

void pst_popup_show_menu(PST_POPUP *popup, HWND owner, HWND anchor,
    const PST_POPUP_ITEM *items, int count, HFONT ui_font)
{
    HWND hwnd;
    HWND child;
    int i;
    int x;
    int y;
    int w;
    int h;
    int cy;
    int max_chars;

    if (popup == 0 || owner == 0 || anchor == 0 || items == 0 || count <= 0) {
        return;
    }
    pst_popup_register();
    pst_popup_close(popup);
    popup->owner = owner;
    popup->ui_font = ui_font;
    max_chars = 14;
    h = 8;
    for (i = 0; i < count; ++i) {
        int n;

        if (items[i].separator) {
            h += 8;
            continue;
        }
        n = (int)strlen(items[i].text != 0 ? items[i].text : "");
        if (items[i].checked) {
            n += 2;
        }
        if (n > max_chars) {
            max_chars = n;
        }
        h += 24;
    }
    w = max_chars * 8 + 34;
    if (w < 180) {
        w = 180;
    }
    pst_popup_place_menu(anchor, w, h, &x, &y);
    hwnd = CreateWindowExA(WS_EX_TOOLWINDOW, PST_POPUP_CLASS, "",
        WS_POPUP | WS_BORDER | WS_VISIBLE,
        x, y, w, h, owner, 0, GetModuleHandleA(0), popup);
    if (hwnd == 0) {
        return;
    }
    popup->hwnd = hwnd;
    cy = 4;
    for (i = 0; i < count; ++i) {
        char label[256];

        if (items[i].separator) {
            child = CreateWindowExA(0, "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
                8, cy + 3, w - 16, 2, hwnd, (HMENU)0,
                GetModuleHandleA(0), 0);
            (void)child;
            cy += 8;
            continue;
        }
        snprintf(label, sizeof(label), "%s%s",
            items[i].checked ? "* " : "  ",
            items[i].text != 0 ? items[i].text : "");
        child = CreateWindowExA(0, PST_POPUP_MENU_ITEM_CLASS, label,
            WS_CHILD | WS_VISIBLE,
            4, cy, w - 8, 22, hwnd, (HMENU)(INT_PTR)items[i].id,
            GetModuleHandleA(0), (LPVOID)(INT_PTR)
                (items[i].checked ? PST_MENU_ITEM_CHECKED : 0));
        pst_popup_set_font(child, ui_font);
        cy += 24;
    }
    SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_SHOWWINDOW);
}
