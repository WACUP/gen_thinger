#ifndef __EMBEDWND_H
#define __EMBEDWND_H

#include <winamp/gen.h>
#include <winamp/wa_ipc.h>
#include "api.h"

#ifndef WINAMP_NEXT_WINDOW
#define WINAMP_NEXT_WINDOW 40063
#endif

#ifndef WINAMP_REFRESHSKIN
#define WINAMP_REFRESHSKIN 40291
#endif

// you can change these values for the section name to store/access the settings
// from winamp.ini and also for the property set when starting Winamp minimised.
#define INI_FILE_SECTION L"Thinger"
#define MINIMISED_FLAG L"ThingerMinMode"

// these functions deal with creating the embedded window and relevant menus, etc
HWND CreateEmbeddedWindow(embedWindowState* embedWindow, const GUID embedWindowGUID);
void AddEmbeddedWindowToMenus(BOOL add, UINT menuId, LPWSTR menuString, BOOL setVisible);
void UpdateEmbeddedWindowsMenu(UINT menuId);
void DestroyEmbeddedWindow(embedWindowState* embedWindow);
BOOL SetEmbeddedWindowMinimizedMode(HWND embeddedWindow, BOOL fMinimized);
BOOL EmbeddedWindowIsMinimizedMode(HWND embeddedWindow);

// these functions are used to process any relevant menu or window messages which the
// embedded window needs to detect inorder to work (especially betweeen instances)
LRESULT HandleEmbeddedWindowChildMessages(HWND embedWnd, UINT menuId, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
void HandleEmbeddedWindowWinampWindowMessages(HWND embedWnd, UINT_PTR menuId, embedWindowState* embedWindow,
											  HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

// shared variables with the embedwnd code this can be altered if you want to use
// different variables or means but is done like this to simplify this example
extern "C" winampGeneralPurposePlugin plugin;
extern BOOL visible, old_visible;

#endif