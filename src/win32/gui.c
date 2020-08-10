#include <windows.h>
#include <stdio.h>
#include "connection.h"
#include "log.h"
#include "zui.h"
#include "gui.h"
#include "error.h"


#define GUI_WIDTH  (640)
#define GUI_HEIGHT (480)

static HWND hwnd_mainwin;
static WNDCLASS wc;
static PAINTSTRUCT paintstruct;
static MSG msg_mainwin;
static BOOL wm_destroy_request = 0;
static DWORD win_width;
static DWORD win_height;

static HDC hdc_mainwin;
static BITMAPINFO bmi = {
	.bmiHeader = {
		.biSize = sizeof(BITMAPINFOHEADER),
		.biWidth = GUI_WIDTH,
		.biHeight = -GUI_HEIGHT,
		.biPlanes = 1,
		.biBitCount = 24,
		.biCompression = BI_RGB,
		.biSizeImage = 0,
		.biXPelsPerMeter = 0,
		.biYPelsPerMeter = 0,
		.biClrUsed = 0,
		.biClrImportant = 0
	}
};

static uint8_t framebuffer[GUI_WIDTH * GUI_HEIGHT * 3];

static int connection_status_text_id;
static CRITICAL_SECTION zui_crit_sect;


static void window_size_update(void)
{
	RECT rect;
	GetClientRect(hwnd_mainwin, &rect);
	win_width = rect.right - rect.left;
	win_height = rect.bottom - rect.top;
}


static void flush_buffer(void)
{
	BeginPaint(hwnd_mainwin, &paintstruct);

	StretchDIBits(hdc_mainwin, 0, 0, win_width, win_height,
	              0, 0, GUI_WIDTH, GUI_HEIGHT,
	              framebuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);


	EndPaint(hwnd_mainwin, &paintstruct);
}


static LRESULT window_proc_clbk(HWND hwnd,
                                UINT msg,
                                WPARAM wParam,
                                LPARAM lParam)
{
	switch (msg) {
		case WM_DESTROY:
			PostQuitMessage(0);
			wm_destroy_request = 1;
			return WM_DESTROY;
		case WM_SIZE:
			window_size_update();
		case WM_PAINT:
			flush_buffer();
			break;
	}	

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool gui_init(void)
{
	#define WINNAME "WiiLinkU " WIILINKU_VER_STR

	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc   = window_proc_clbk;
	wc.lpszClassName = WINNAME;

	RegisterClass(&wc);

	hwnd_mainwin = CreateWindowEx(
			0, wc.lpszClassName,
			WINNAME, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			NULL, NULL, wc.hInstance, NULL);

	if (!hwnd_mainwin) {
		set_last_error("CreateWindowEx failed");
		return false;
	}

	ShowWindow(hwnd_mainwin, 1);
	UpdateWindow(hwnd_mainwin);
	window_size_update();

	hdc_mainwin = GetDC(hwnd_mainwin);

	char title_buffer[96];
	sprintf(title_buffer,"HOST IP %s", connection_get_host_address());

	zui_init(title_buffer, framebuffer, GUI_WIDTH, GUI_HEIGHT);

	connection_status_text_id = zui_dynamic_text_create(0, 64, 1);

	InitializeCriticalSectionAndSpinCount(&zui_crit_sect, ~(DWORD)0);

	gui_set_connection_status(false, NULL);

	return true;
}

void gui_term(void)
{
	DeleteCriticalSection(&zui_crit_sect);
	zui_term();
	DestroyWindow(hwnd_mainwin);
}

void gui_set_connection_status(bool connected, const char* clientaddr)
{
	EnterCriticalSection(&zui_crit_sect);
	if (!connected) {
		zui_dynamic_text_set(
			connection_status_text_id,
			"NOT CONNECTED",
			(struct vec2i) {
				GUI_WIDTH / 2,
				GUI_HEIGHT / 2
			}
		);
	} else {
		char buf[64];
		sprintf(buf, "!! CONNECTED TO: %s !!", clientaddr);
		zui_dynamic_text_set(
			connection_status_text_id,
			buf,
			(struct vec2i) {
				GUI_WIDTH / 2,
				GUI_HEIGHT / 2
			}
		);
	}
	LeaveCriticalSection(&zui_crit_sect);
}

gui_event_t gui_update(void)
{
	if (wm_destroy_request)
		return GUI_EVENT_WM_DESTROY;

	while (PeekMessageA(&msg_mainwin, hwnd_mainwin, 0, 0, PM_REMOVE))
		DispatchMessage(&msg_mainwin);


	EnterCriticalSection(&zui_crit_sect);
	if (zui_update()) {
		zui_render();
		flush_buffer();
	}
	
	LeaveCriticalSection(&zui_crit_sect);

	return 0;
}


