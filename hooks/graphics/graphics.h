#pragma once

#include <string>
#include <vector>
#include <optional>

#include <windows.h>
#include <d3d9.h>

#include "external/toojpeg/toojpeg.h"

// flag settings
extern bool GRAPHICS_CAPTURE_CURSOR;
extern bool GRAPHICS_LOG_HRESULT;
extern bool GRAPHICS_SDVX_FORCE_720;
extern bool GRAPHICS_SHOW_CURSOR;
extern bool GRAPHICS_WINDOWED;
extern bool GRAPHICS_ADJUST_ORIENTATION;
extern std::vector<HWND> GRAPHICS_WINDOWS;
extern UINT GRAPHICS_FORCE_REFRESH;
extern bool GRAPHICS_FORCE_SINGLE_ADAPTER;
extern bool GRAPHICS_9_ON_12;

// settings
extern std::string GRAPHICS_DEVICEID;
extern std::string GRAPHICS_SCREENSHOT_DIR;

// Direct3D 9 settings
extern std::optional<UINT> D3D9_ADAPTER;
extern DWORD D3D9_BEHAVIOR_DISABLE;

void graphics_init();
void graphics_hook_window(HWND hWnd, D3DPRESENT_PARAMETERS *pPresentationParameters);
void graphics_add_wnd_proc(WNDPROC wndProc);
void graphics_remove_wnd_proc(WNDPROC wndProc);
void graphics_screens_register(int screen);
void graphics_screens_unregister(int screen);
void graphics_screens_get(std::vector<int> &screens);
void graphics_screenshot_trigger();
bool graphics_screenshot_consume();
void graphics_capture_trigger(int screen);
bool graphics_capture_consume(int *screen);
void graphics_capture_enqueue(int screen, uint8_t *data, size_t width, size_t height);
void graphics_capture_skip(int screen);
bool graphics_capture_receive_jpeg(int screen, TooJpeg::WRITE_ONE_BYTE receiver,
        bool rgb = true, int quality = 80, bool downsample = true, int divide = 0,
        uint64_t *timestamp = nullptr,
        int *width = nullptr, int *height = nullptr);
std::string graphics_screenshot_genpath();
