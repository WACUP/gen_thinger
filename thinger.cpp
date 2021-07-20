/* Thinger plugin for Winamp 2.x/5.x
 * Author: Saivert
 * Homepage: http://inthegray.com/saivert/
 * E-Mail: saivert@gmail.com
 *
 * BUILD NOTES:
 *  Before building this project, please check the post-build step in the project settings.
 *  You must make sure the path that the file is copied to is referring to where you have your
 *  Winamp plugins directory.
 */


// #define this to use the TransparentBlt GDI function
#define USE_TRANSPARENTBLT

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>

#include <winamp/gen.h>
#include <winamp/wa_cup.h>
#include <winamp/wa_msgids.h>
#include <winamp/wa_hotkeys.h>
#include <winamp/wa_dlg.h>

#include <loader/loader/paths.h>
#include <loader/loader/utils.h>
#include <loader/loader/ini.h>
#include <loader/hook/BitmapEx.h>

#include "iconlist.h"
#include "embedwnd.h"
#include "resource.h"

#include <api/memmgr/api_memmgr.h>

/* global data */
#define PLUGIN_INISECTION TEXT("Thinger")
#define PLUGIN_VERSION "1.0.1"

// Menu ID's
UINT WINAMP_NXS_THINGER_MENUID = 48882;

/* Repeat delay in ms */
#define SCROLLREPEATDELAY 60
/* How many pixels to shift each scroll */
#define SCROLLAMOUNT 5

/* Internal icons */
#define IDC_PLEDIT  32001
#define IDC_ML      32002
#define IDC_VIDEO   32003
#define IDC_VIS     32004
#define IDC_PREFS   32005
#define IDC_NEW     32006 //placeholder

// Internal Thinger icon flags
// New in v0.515
#define THINGER_PLEDIT  2
#define THINGER_ML      4
#define THINGER_VIDEO   8
#define THINGER_VIS     16
#define THINGER_PREFS   32
#define THINGER_NEW     64 //placeholder

static ATOM wndclass = 0;
static HWND g_thingerwnd = NULL;
#ifdef _DEBUG
static BOOL g_debugmode = FALSE;
#endif
static HMENU g_hPopupMenu = NULL;
static int g_scrolloffset = 0;
static LPARAM ipc_thingerinit = -1;

static HFONT g_hStatusFont = NULL;

/* Thinger API stuff */
static LONG g_thingeripc = -1;

/* New in v0.517: Default bitmaps for invalid bitmap/icon handles */
static HBITMAP g_hbmDef = NULL;
static HBITMAP g_hbmDefHighlight = NULL;

static HICON g_hiDef = NULL;
static HICON g_hiDefHighlight = NULL;

static int upscaling = 1, dsize = 0, no_uninstall = 1;
static HWND hWndThinger = NULL;

/* Thinger window */
static embedWindowState embed = { 0 };
static genHotkeysAddStruct genhotkey = { 0 };

LRESULT CALLBACK ThingerWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK GenWndSubclass(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK WinampSubclass(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK ButtonSubclass(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

RECT GetIconRect(int index);
int GetIconFromPoint(POINT pt);
int GetNumVisibleIcons();

api_language *WASABI_API_LNG = NULL;
// these two must be declared as they're used by the language api's
// when the system is comparing/loading the different resources
HINSTANCE WASABI_API_LNG_HINST = NULL, WASABI_API_ORIG_HINST = NULL;

// this is used to identify the skinned frame to allow
// for embedding/control by modern skins if needed
// {9D97A8C3-01E2-4648-9878-64DDD164E63A}
static const GUID embed_guid =
{ 0x9d97a8c3, 0x1e2, 0x4648, { 0x98, 0x78, 0x64, 0xdd, 0xd1, 0x64, 0xe6, 0x3a } };

#ifndef USE_TRANSPARENTBLT
  void DrawTransparentBitmap(HDC hDC, HBITMAP hBitmap, int x, int y, COLORREF crColour);
#endif

/* configuration items */
static int config_showsb=TRUE;
static int config_hideinticons=0; // New in v0.515
static int config_dimmedonscroll=0; // New in v0.518

/* plugin function prototypes */
void config(void);
void quit(void);
int init(void);

void __cdecl MessageProc(HWND hWnd, const UINT uMsg, const
						 WPARAM wParam, const LPARAM lParam);

winampGeneralPurposePlugin plugin =
{
	GPPHDR_VER_WACUP,
	(char*)L"Thinger",
	init, config, quit,
	GEN_INIT_WACUP_HAS_MESSAGES
};

void ProcessMenuResult(UINT command, HWND parent) {
	switch (command) {
#ifdef _DEBUG
		case 10: {
			g_debugmode = !g_debugmode;
		}
		break;
		case 11: {
			lpNxSThingerIconStruct lpntis = IconList_GetFromID(g_uLastIconID);
			if (lpntis) {
				char s[256];
				wsprintf(s, "flags: %ld\r\ndesc: \"%s\"\r\nicon: %ld\r\nhwnd: %ld\r\n"
					"msg: %ld\r\nwParam: %ld\r\nlParam: %ld",
					lpntis->dwFlags, lpntis->lpszDesc, lpntis->hIcon, lpntis->hWnd,
					lpntis->uMsg, lpntis->wParam, lpntis->lParam);
				MessageBox(hDlg, s, "DEBUG: Icon info", 64);
			}
		}
		break;
#endif
		case 20:
		case 21:
		case 22:
		case 23: 
		case 24: {
			/* to account for some aspects not being prsent it's necessary to adjust */
			/* what we're actually going to remove to avoid the wrong icon changing */
			const int flag[] = { THINGER_PLEDIT, THINGER_ML, THINGER_VIDEO, THINGER_VIS, THINGER_PREFS };
			int which = (command - 20), real_which = which;
			if (which >= 1)
			{
				if (!HasML())
				{
					--which;
				}
			}

			if (which >= 2)
			{
				if (!HasVideo())
				{
					--which;
				}
			}

			lpNxSThingerIconStruct lpntis = IconList_Get(which);
			if (lpntis) {
				lpntis->dwFlags ^= NTIS_HIDDEN;
			}

			config_hideinticons ^= flag[real_which];
			if (config_hideinticons) {
				SaveNativeIniInt(PLUGIN_INI, PLUGIN_INISECTION, L"config_hideinticons", config_hideinticons);
			} else {
				SaveNativeIniString(PLUGIN_INI, PLUGIN_INISECTION, L"config_hideinticons", NULL);
			}
			InvalidateRect(g_thingerwnd, NULL, TRUE);
			break;
		}
		case ID__SHOWSTATUSBAR: {
			config_showsb = !config_showsb;
			SaveNativeIniInt(PLUGIN_INI, PLUGIN_INISECTION, L"config_showsb", config_showsb);

			/* help force update the view without manually resizing like the plug-in required */
			RECT r = { 0 };
			GetWindowRect(hWndThinger, &r);
			SetWindowPos(hWndThinger, NULL, 0, 0, (r.right - r.left),
						 (r.bottom - r.top), SWP_NOACTIVATE |
						 SWP_NOMOVE | SWP_NOZORDER);
			InvalidateRect(g_thingerwnd, NULL, TRUE);
			break;
		}
		case ID__DIMICONSWHENSCROLLING: {
			config_dimmedonscroll = !config_dimmedonscroll;				
			SaveNativeIniInt(PLUGIN_INI, PLUGIN_INISECTION, L"config_dimmedonscroll", config_dimmedonscroll);
			break;
		}
		case ID__ABOUT: {
			wchar_t message[2048] = { 0 };
			StringCchPrintf(message, ARRAYSIZE(message), WASABI_API_LNGSTRINGW(IDS_ABOUT_STRING), TEXT(__DATE__));
			AboutMessageBox(plugin.hwndParent, message, (LPWSTR)plugin.description);
			break;
		}
	}
}

void SetupConfigMenu(HMENU popup)
{
#ifdef _DEBUG
	EnableMenuItem(popup, 11, MF_BYCOMMAND | (g_uLastIconID != -1) ? MF_ENABLED : MF_GRAYED);
	CheckMenuItem(popup, 10, MF_BYCOMMAND | g_debugmode ? MF_CHECKED : MF_UNCHECKED);
#endif

	HMENU sub_menu = GetSubMenu(popup, 0);

	// Check the "Show built-in icons" menu items
	BOOL checked = ((config_hideinticons & THINGER_PLEDIT) != THINGER_PLEDIT);
	CheckMenuItem(popup, 20, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));

	checked = ((config_hideinticons & THINGER_ML) != THINGER_ML);
	CheckMenuItem(popup, 21, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
	EnableMenuItem(sub_menu, 21, MF_BYCOMMAND | (!!HasML() ? MF_ENABLED : MF_GRAYED));

	checked = ((config_hideinticons & THINGER_VIDEO) != THINGER_VIDEO);
	CheckMenuItem(popup, 22, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
	EnableMenuItem(sub_menu, 22, MF_BYCOMMAND | (HasVideo() ? MF_ENABLED : MF_GRAYED));

	checked = ((config_hideinticons & THINGER_VIS) != THINGER_VIS);
	CheckMenuItem(popup, 23, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));

	checked = ((config_hideinticons & THINGER_PREFS) != THINGER_PREFS);
	CheckMenuItem(popup, 24, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));

	CheckMenuItem(popup, ID__SHOWSTATUSBAR, MF_BYCOMMAND | (config_showsb ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(popup, ID__DIMICONSWHENSCROLLING, MF_BYCOMMAND | (config_dimmedonscroll ? MF_CHECKED : MF_UNCHECKED));
}

void config(void) {
	//HMENU hMenu = WASABI_API_LOADMENUW(IDR_CONTEXTMENU);
	HMENU popup = GetSubMenu(LoadMenu(plugin.hDllInstance, TEXT("MENU1")), 0);
	RECT r = { 0 };

	AddItemToMenu2(popup, 0, (LPWSTR)plugin.description, 0, 1);
	EnableMenuItem(popup, 0, MF_BYCOMMAND | MF_GRAYED | MF_DISABLED);
	AddItemToMenu2(popup, 0, 0, 1, 1);

	HWND list = FindWindowEx(GetParent(GetFocus()), 0, L"SysListView32", 0);
	ListView_GetItemRect(list, ListView_GetSelectionMark(list), &r, LVIR_BOUNDS);
	ClientToScreen(list, (LPPOINT)&r);

	SetupConfigMenu(popup);

	ProcessMenuResult(TrackPopupMenu(popup, TPM_RETURNCMD | TPM_LEFTBUTTON,
									 r.left, r.top, 0, list, NULL), list);
	DestroyMenu(popup);
}

void quit(void) {

	IconList_Free();

	UnSubclass(plugin.hwndParent, WinampSubclass);

	if (no_uninstall)
	{
		/* Update position and size */
		DestroyEmbeddedWindow(&embed);
	}

	if (IsWindow(g_thingerwnd)) {
		DestroyWindow(g_thingerwnd); /* delete our window */
	}

	if (IsWindow(embed.me)) {
		DestroyWindow(embed.me);
	}
}

int init(void) {
	WASABI_API_LNG = plugin.language;/**/
	// TODO add to lang.h
	WASABI_API_START_LANG(plugin.hDllInstance, embed_guid);

	wchar_t pluginTitleW[256] = { 0 };
	StringCchPrintf(pluginTitleW, ARRAYSIZE(pluginTitleW), WASABI_API_LNGSTRINGW(IDS_PLUGIN_NAME), TEXT(PLUGIN_VERSION));
	plugin.description = (char*)_wcsdup(pluginTitleW);

	// wParam must have something provided else it returns 0
	// and then acts like a IPC_GETVERSION call... not good!
	ipc_thingerinit = RegisterIPC((WPARAM)&"Thinger_IPC");
	PostMessage(plugin.hwndParent, WM_WA_IPC, 0, ipc_thingerinit);

	return GEN_INIT_SUCCESS;
}

WA_UTILS_API HBITMAP ResizeBitmap(HBITMAP hbmp, const INT cx,
								  const INT cy, const BOOL mode);

int Scale(const int mode)
{
	#define WND_CXICON 36
	#define WND_CXICON_FH 48
	#define WND_CYICON 36
	#define WND_CYICON_FH 48
	#define WND_CXBTN 14
	#define WND_CYSB 14
	return (!mode ? (config_showsb ? WND_CXICON : WND_CXICON_FH) :
		   (mode == 1 ? (config_showsb ? WND_CYICON : WND_CYICON_FH) :
		   (mode == 2 ? WND_CXBTN : WND_CYSB))) * (dsize + 1);
	#undef WND_CXICON
	#undef WND_CYICON
	#undef WND_CXBTN
	#undef WND_CYSB
}

HBITMAP LoadPngFromBMP(const UINT ctrl_img)
{
	int cur_w = 0, cur_h = 0;
	ARGB32 *data = LoadImageFromResource(plugin.language, WASABI_API_LNG_HINST,
										 WASABI_API_ORIG_HINST, ctrl_img,
										 L"PNG", &cur_w, &cur_h);
	// TODO consider doing something to improve upscaling
	HBITMAP ret = ResizeBitmap(GetHBitmap(data, cur_w, cur_h, 0, 0, -1), Scale(0), Scale(1), 0);
	plugin.memmgr->sysFree(data);
	return ret;
}

void AddIcon(NxSThingerIconStruct &ntis, const int flag, LPCWSTR text,
			 const int param, const int icon_id, const int icon_h_id)
{
	if ((config_hideinticons & flag) == flag)
		ntis.dwFlags = NTIS_BITMAP | NTIS_HIDDEN;
	else
		ntis.dwFlags = NTIS_BITMAP;

	ntis.lpszDesc = text;
	ntis.wParam = MAKEWPARAM(param, 0);
	ntis.hBitmap = LoadPngFromBMP(icon_id);
	ntis.hBitmapHighlight = LoadPngFromBMP(icon_h_id);
	IconList_Add(&ntis);
}

void UpdateStatusFont(void) {
	if (g_hStatusFont != NULL) {
		DeleteObject(g_hStatusFont);
	}

	g_hStatusFont = CreateFont(-8, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
							   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
							   DEFAULT_PITCH | FF_DONTCARE, (LPCWSTR)GetGenSkinBitmap(6));

	RECT rc = { 0 };
	GetClientRect(hWndThinger, &rc);
	GetIdealFontForArea(hWndThinger, g_hStatusFont, (rc.right - rc.left), Scale(3));
	SendMessage(GetDlgItem(hWndThinger, IDC_STATUS), WM_SETFONT,
				(WPARAM)g_hStatusFont, MAKELPARAM(1, 0));
}

/* to avoid subclassing this will get a reasonable set of IPC messages */
void __cdecl MessageProc(HWND hWnd, const UINT uMsg, const
						 WPARAM wParam, const LPARAM lParam)
{
	if (uMsg == WM_WA_IPC) {
		/* Init time */
		if (lParam == ipc_thingerinit) {
			dsize = GetDoubleSize(&upscaling);

			config_showsb = GetNativeIniInt(PLUGIN_INI, PLUGIN_INISECTION, L"config_showsb", config_showsb);
			config_hideinticons = GetNativeIniInt(PLUGIN_INI, PLUGIN_INISECTION, L"config_hideinticons", config_hideinticons);
			config_dimmedonscroll = GetNativeIniInt(PLUGIN_INI, PLUGIN_INISECTION, L"config_dimmedonscroll", config_dimmedonscroll);

			// for the purposes of this example we will manually create an accelerator table so
			// we can use IPC_REGISTER_LOWORD_COMMAND to get a unique id for the menu items we
			// will be adding into Winamp's menus. using this api will allocate an id which can
			// vary between Winamp revisions as it moves depending on the resources in Winamp.
			WINAMP_NXS_THINGER_MENUID = RegisterCommandID(0);

			// then we show the embedded window which will cause the child window to be
			// sized into the frame without having to do any thing ourselves. also this will
			// only show the window if Winamp was not minimised on close and the window was
			// open at the time otherwise it will remain hidden
			old_visible = visible = GetNativeIniInt(PLUGIN_INI, PLUGIN_INISECTION, L"config_show", TRUE);

			// finally we add menu items to the main right-click menu and the views menu
			// with Modern skins which support showing the views menu for accessing windows
			AddEmbeddedWindowToMenus(TRUE, WINAMP_NXS_THINGER_MENUID,
									 WASABI_API_LNGSTRINGW(IDS_NXS_THINGER), -1);

			// now we will attempt to create an embedded window which adds its own main menu entry
			// and related keyboard accelerator (like how the media library window is integrated)
			embed.flags |= EMBED_FLAGS_SCALEABLE_WND | EMBED_FLAGS_NO_CHILD_SIZING;	// double-size support!
			hWndThinger = CreateEmbeddedWindow(&embed, embed_guid);

			/* Subclass skinned window frame */
			Subclass(hWndThinger, GenWndSubclass);

			// once the window is created we can then specify the window title and menu integration
			SetWindowText(hWndThinger, WASABI_API_LNGSTRINGW(IDS_NXS_THINGER));

			WNDCLASSEX wcex = { 0 };
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.lpszClassName = TEXT("NxSThingerWnd");
			wcex.hInstance = plugin.hDllInstance;
			wcex.lpfnWndProc = ThingerWndProc;
			wndclass = RegisterClassEx(&wcex);
			if (wndclass) {
				RECT r = { 0 };
				GetWindowRect(hWndThinger, &r);

				MLSKINWINDOW sw = { 0 };
				sw.style = /*SWS_USESKINFONT |*/ SWS_USESKINCOLORS | SWS_USESKINCURSORS;
				sw.skinType = SKINNEDWND_TYPE_BUTTON;

				HWND button = CreateWindow(WC_BUTTON, L"<", WS_CHILD | WS_TABSTOP |
										   WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWndThinger,
										   (HMENU)IDC_LEFTSCROLLBTN, plugin.hDllInstance, 0);
				if (IsWindow(button))
				{
					sw.hwndToSkin = button;
					SkinWindow(&sw);

					/* Subclass buttons so we get notified when they are down/up */
					Subclass(button, ButtonSubclass);
				}

				g_thingerwnd = CreateWindow((LPCTSTR)wndclass, PLUGIN_INISECTION, WS_CHILD | WS_VISIBLE,
											0, 0, 0, 0, hWndThinger, NULL, plugin.hDllInstance, NULL);

				button = CreateWindow(WC_BUTTON, L">", WS_CHILD | WS_TABSTOP |
									  WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0, hWndThinger,
									  (HMENU)IDC_RIGHTSCROLLBTN, plugin.hDllInstance, 0);
				if (IsWindow(button))
				{
					sw.hwndToSkin = button;
					SkinWindow(&sw);

					/* Subclass buttons so we get notified when they are down/up */
					Subclass(button, ButtonSubclass);
				}

				button = CreateWindow(WC_STATIC, 0, WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
									  0, 0, 0, 0, hWndThinger, (HMENU)IDC_STATUS, (HINSTANCE)0, 0);
				if (IsWindow(button)) {
					sw.skinType = SKINNEDWND_TYPE_STATIC;
					sw.hwndToSkin = button;
					SkinWindow(&sw);

					/* Subclass buttons so we get notified when they are down/up */
					Subclass(button, ButtonSubclass);
				}

				UpdateStatusFont();
			}

			// Winamp can report if it was started minimised which allows us to control our window
			// to not properly show on startup otherwise the window will appear incorrectly when it
			// is meant to remain hidden until Winamp is restored back into view correctly
			if (InitialShowState() == SW_SHOWMINIMIZED)
			{
				SetEmbeddedWindowMinimizedMode(hWndThinger, TRUE);
			}
			else
			{
				// only show on startup if under a classic skin and was set
				if (visible)
				{
					PostMessage(hWndThinger, WM_USER + 102, 0, 0);
				}
			}

			/* Initialize our Icon List */
			IconList_Init();

			/* Register our IPC message (for the Thinger API) */
			g_thingeripc = RegisterIPC((WPARAM)&"NxSThingerIPCMsg");

			/* Subclass Winamp's main window */
			Subclass(plugin.hwndParent, WinampSubclass);

			/* Load the default bitmaps and icons */
			g_hbmDef = LoadBitmap(plugin.hDllInstance, MAKEINTRESOURCE(IDB_NEW));
			g_hbmDefHighlight = LoadBitmap(plugin.hDllInstance, MAKEINTRESOURCE(IDB_NEW_H));

			g_hiDef = LoadIcon(plugin.hDllInstance, MAKEINTRESOURCE(IDI_NEW));
			g_hiDefHighlight = LoadIcon(plugin.hDllInstance, MAKEINTRESOURCE(IDI_NEW_H));

			/* Add the built-in icons to the icon list */
			/* Set common values */
			NxSThingerIconStruct ntis = { 0 };
			ntis.dwFlags = /*NTIS_ADD |*/ NTIS_BITMAP;
			ntis.hWnd = g_thingerwnd;
			ntis.uMsg = WM_COMMAND;

			/* Start adding icons to list */
			/* New in v0.515: Checks for presence of a flag in config_hideinticons which determines
			   if the icons are hidden or not. */
			AddIcon(ntis, THINGER_PLEDIT, TEXT("Playlist Editor"), IDC_PLEDIT, IDB_PLEDIT, IDB_PLEDIT_H);

			/* Check if the Media Library core is present */
			if (HasML()) {
				AddIcon(ntis, THINGER_ML, TEXT("Media Library"), IDC_ML, IDB_ML, IDB_ML_H);
			}

			if (HasVideo()) {
				AddIcon(ntis, THINGER_VIDEO, TEXT("Video"), IDC_VIDEO, IDB_VIDEO, IDB_VIDEO_H);
			}

			AddIcon(ntis, THINGER_VIS, TEXT("Toggle Visualisation"), IDC_VIS, IDB_VIS, IDB_VIS_H);

			AddIcon(ntis, THINGER_PREFS, TEXT("Toggle Preferences"), IDC_PREFS, IDB_PREFS, IDB_PREFS_H);

			/* Get message value */
			UINT genhotkeys_add_ipc = RegisterIPC((WPARAM)&"GenHotkeysAdd");

			/* Set up the genHotkeysAddStruct */
			genhotkey.name = (char*)WASABI_API_LNGSTRINGW_DUP(IDS_GHK_STRING);
			genhotkey.flags = HKF_NOSENDMSG | HKF_UNICODE_NAME;
			genhotkey.id = "NxSThingerToggle";
			// get this to send a WM_COMMAND message so we don't have to do anything specific
			genhotkey.uMsg = WM_COMMAND;
			genhotkey.wParam = MAKEWPARAM(WINAMP_NXS_THINGER_MENUID, 0);
			genhotkey.lParam = 0;
			genhotkey.wnd = g_thingerwnd;
			PostMessage(plugin.hwndParent, WM_WA_IPC, (WPARAM)&genhotkey, genhotkeys_add_ipc);
		}
		else if (lParam == IPC_SKIN_CHANGED_NEW) {
			// make sure we catch all appropriate skin changes
			UpdateStatusFont();
			InvalidateRect(g_thingerwnd, NULL, TRUE);
		}
	}
	
	// this will handle the message needed to be caught before the original window
	// proceedure of the subclass can process it. with multiple windows then this
	// would need to be duplicated for the number of embedded windows your handling
	HandleEmbeddedWindowWinampWindowMessages(hWndThinger, WINAMP_NXS_THINGER_MENUID,
											 &embed, hWnd, uMsg, wParam, lParam);
}

/* Subclass used to intercept our IPC messages and be
   notified when someone clicks the lightning bolt. */   
LRESULT CALLBACK WinampSubclass(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
								UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	/* Check for our IPC message */
	if ((uMsg == WM_WA_IPC) && (lParam == g_thingeripc)) {
		lpNxSThingerIconStruct lpntis=(lpNxSThingerIconStruct)wParam;

#ifdef _DEBUG
		char s[256];
		if (g_debugmode)
			wsprintf(s, "flags: %ld\r\ndesc: \"%s\"\r\nicon: %ld\r\nhwnd: %ld\r\n"
				"msg: %ld\r\nwParam: %ld\r\nlParam: %ld",
				lpntis->dwFlags, lpntis->lpszDesc, lpntis->hIcon, lpntis->hWnd,
				lpntis->uMsg, lpntis->wParam, lpntis->lParam);
#endif

		if ((lpntis->dwFlags & NTIS_ADD) == NTIS_ADD) {
			unsigned int i=0;
			/* Safai was here! :-) */
			if (lpntis->hWnd == NULL) lpntis->hWnd = plugin.hwndParent;
			if (lpntis->hIcon && IsWindow(lpntis->hWnd)) {
				/* Substitute invalid icon/bitmap with a default one */
				if ((lpntis->dwFlags & NTIS_BITMAP) == NTIS_BITMAP) {
					if (!GetObjectType(lpntis->hBitmap))
						lpntis->hBitmap=g_hbmDef;

					if (!GetObjectType(lpntis->hBitmapHighlight))
						lpntis->hBitmapHighlight=g_hbmDefHighlight;
				} else {
					if (!GetObjectType(lpntis->hIcon))
						lpntis->hIcon=g_hiDef;

					if (!GetObjectType(lpntis->hIconHighlight))
						lpntis->hIconHighlight=g_hiDefHighlight;
				}

				/* Now add the damn icon */
				i = IconList_Add(lpntis);
			}
#ifdef _DEBUG
			if (g_debugmode)
				MessageBox(hwnd, s, "DEBUG: Got Add request!", 64);
#endif
			InvalidateRect(g_thingerwnd, NULL, TRUE);
			return i;
		} else if ((lpntis->dwFlags & NTIS_MODIFY) == NTIS_MODIFY) {
			lpNxSThingerIconStruct lpntisCur = IconList_GetFromID(lpntis->uIconId);
			if (lpntisCur != NULL) {

				if ((lpntis->dwFlags & NTIS_HIDDEN) == NTIS_HIDDEN) {
					lpntisCur->dwFlags |= NTIS_HIDDEN;
				} else {
					lpntisCur->dwFlags &= ~NTIS_HIDDEN;
				}

				if ((lpntis->dwFlags & NTIS_NODESC) != NTIS_NODESC)
					lpntisCur->lpszDesc = lpntis->lpszDesc;


				if ((lpntis->dwFlags & NTIS_NOICON) != NTIS_NOICON) {
					if ((lpntis->dwFlags & NTIS_BITMAP) == NTIS_BITMAP) {
						lpntisCur->dwFlags |= NTIS_BITMAP;

						lpntisCur->hBitmap = lpntis->hBitmap;
						lpntisCur->hBitmapHighlight = lpntis->hBitmapHighlight;
					} else {
						lpntisCur->dwFlags &= ~NTIS_BITMAP;

						lpntisCur->hIcon = lpntis->hIcon;
						lpntisCur->hIconHighlight = lpntis->hIconHighlight;
					}
				}
			}
#ifdef _DEBUG
			if (g_debugmode)
				MessageBox(hwnd, s, "DEBUG: Got Modify request!", 64);
#endif
			InvalidateRect(g_thingerwnd, NULL, TRUE);
			if (lpntisCur) return lpntisCur->uIconId;
		} else if ((lpntis->dwFlags & NTIS_DELETE)==NTIS_DELETE) {
			IconList_DelWithID(lpntis->uIconId);
#ifdef _DEBUG
			if (g_debugmode)
				MessageBox(hwnd, s, "DEBUG: Got Delete request!", 64);
#endif
			InvalidateRect(g_thingerwnd, NULL, TRUE);
		}
	}

	/* Override action for Lightning bolt icon */
	if (uMsg==WM_COMMAND && LOWORD(wParam)==WINAMP_LIGHTNING_CLICK &&
		(GetLibraryIniInt(L"attachlbolt", 0) == 3)) {
		PostMessage(hwnd, WM_COMMAND, WINAMP_NXS_THINGER_MENUID, 0);
		return 0;
	}

	return DefSubclass(hwnd, uMsg, wParam, lParam);
}

/* New in v0.4: Calculates a rectangle based on index. Not aware of hidden icons,
   simply returns the rectangle of a "icon" at the given index.
   Only used by the "GetIconFromPoint" function. */
RECT GetIconRect(int index) {
	const int offset = Scale(0);
	RECT tr;
	tr.left = (Scale(2) + (index * offset)) + g_scrolloffset;
	tr.top = 0;
	tr.right = (tr.left + offset) - g_scrolloffset;
	tr.bottom = tr.top + Scale(1);
	return tr;
}

/* Fixed in v0.5: Accounts for hidden icons.
   Returns the index of the icon at point pt. */
int GetIconFromPoint(POINT pt) {
	RECT r = { 0 };
	int c = IconList_GetSize(),
		i = 0,
		n = 0;
	while (i < c) {
		if ((IconList_Get(i)->dwFlags & NTIS_HIDDEN)!=NTIS_HIDDEN) {
			r = GetIconRect(n);
			if (PtInRect(&r, pt))
				return i;
			++n;
		}
		++i;
	}
	return -1;
}

/* New in v0.512: Returns the number of visible icons */
int GetNumVisibleIcons() {
	int c = IconList_GetSize(),
		n = 0;
	for (int i=0;i<c;i++) {
		if ((IconList_Get(i)->dwFlags & NTIS_HIDDEN)!=NTIS_HIDDEN) ++n;
	}
	
	return n;
}

/* New in v0.4: Subclass used to make the buttons notify it's parent when down/up. */
LRESULT CALLBACK ButtonSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
								UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (uMsg) {
	case WM_LBUTTONDOWN:
		SendMessage(g_thingerwnd, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), WM_LBUTTONDOWN), (LPARAM)hWnd);
		break;
	case WM_LBUTTONUP:
		SendMessage(g_thingerwnd, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(hWnd), WM_LBUTTONUP), (LPARAM)hWnd);
		break;
	}

	return DefSubclass(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ThingerWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	// if you need to do other message handling then you can just place this first and
	// process the messages you need to afterwards. note this is passing the frame and
	// its id so if you have a few embedded windows you need to do this with each child
	if (HandleEmbeddedWindowChildMessages(hWndThinger, WINAMP_NXS_THINGER_MENUID,
										  hWnd, uMsg, wParam, lParam)) {
		return 0;
	}

	static UINT lastItem=0;
	static int iHighlightItem=-1;
	static UINT g_uLastIconID=0;
	static BOOL buttonsDown=FALSE;

	switch (uMsg) {
	case WM_COMMAND: {
		switch (LOWORD(wParam)) {
			case IDC_LEFTSCROLLBTN: {
				if (HIWORD(wParam) == WM_LBUTTONDOWN) {
					SetTimer(hWnd, IDC_LEFTSCROLLBTN, SCROLLREPEATDELAY, NULL);
					buttonsDown = TRUE;
				}
				if (HIWORD(wParam) == WM_LBUTTONUP) {
					KillTimer(hWnd, IDC_LEFTSCROLLBTN);
					buttonsDown = FALSE;
					InvalidateRect(g_thingerwnd, NULL, TRUE);
				}
				break;
			}
			case IDC_RIGHTSCROLLBTN: {
				if (HIWORD(wParam) == WM_LBUTTONDOWN) {
					SetTimer(hWnd, IDC_RIGHTSCROLLBTN, SCROLLREPEATDELAY, NULL);
					buttonsDown = TRUE;
				}
				if (HIWORD(wParam) == WM_LBUTTONUP) {
					KillTimer(hWnd, IDC_RIGHTSCROLLBTN);
					buttonsDown = FALSE;
					InvalidateRect(g_thingerwnd, NULL, TRUE);
				}
				break;
			}
			case IDC_PLEDIT: {
				PostMessage(plugin.hwndParent, WM_COMMAND, WINAMP_OPTIONS_PLEDIT, 0);
				break;
			}
			case IDC_ML: {
				if (IsWindowVisible(GetMLWindow())) {
					PostMessage(plugin.hwndParent, WM_COMMAND, ID_FILE_CLOSELIBRARY, 0);
				}
				else {
					PostMessage(plugin.hwndParent, WM_COMMAND, ID_FILE_SHOWLIBRARY, 0);
				}
				break;
			}
			case IDC_VIDEO: {
				PostMessage(plugin.hwndParent, WM_COMMAND, WINAMP_OPTIONS_VIDEO, 0);
				break;
			}
			case IDC_VIS: {
				PostMessage(plugin.hwndParent, WM_COMMAND, WINAMP_VISPLUGIN, 0);
				break;
			}
			case IDC_PREFS: {
				PostMessage(plugin.hwndParent, WM_COMMAND, WINAMP_OPTIONS_PREFS, 0);
				break;
			}
			break;
		}
	}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP: {
		POINT pt = { 0 };
		GetCursorPos(&pt);
		ScreenToClient(hWnd, &pt);
		pt.x -= g_scrolloffset;
		const int i = GetIconFromPoint(pt);
		if (i >= 0 && i<IconList_GetSize()) {
			lpNxSThingerIconStruct lpntis = IconList_Get(i);
			if (lpntis) {
				g_uLastIconID = lpntis->uIconId;
				if (uMsg == WM_LBUTTONUP)
					PostMessage(lpntis->hWnd, lpntis->uMsg, lpntis->wParam, lpntis->lParam);
			}
		} else {
			g_uLastIconID = (UINT)-1;
		}
		break;
	}
	case WM_MOUSEMOVE: {
		int i;
		POINT pt;
		lpNxSThingerIconStruct lpntis;
		RECT r;
		TRACKMOUSEEVENT tme;

		GetCursorPos(&pt);
		ScreenToClient(hWnd, &pt);
		pt.x -= g_scrolloffset;
		i = GetIconFromPoint(pt);

		if (iHighlightItem!=i) {
			r = GetIconRect(iHighlightItem);
			InvalidateRect(hWnd, &r, TRUE);
		}

		if (i >= 0 && i < IconList_GetSize()) {
			lpntis = IconList_Get(i);
			if (lpntis) {
				SetDlgItemText(hWndThinger, IDC_STATUS, lpntis->lpszDesc);
			}
			if (iHighlightItem!=i) {
				r = GetIconRect(i);
				InvalidateRect(hWnd, &r, TRUE);
			}
		} else {
			SetDlgItemText(hWndThinger, IDC_STATUS, TEXT(""));
		}
		iHighlightItem = i;

		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = /*TME_HOVER|*/TME_LEAVE;
		tme.hwndTrack = hWnd;
		tme.dwHoverTime = 0;
		TrackMouseEvent(&tme);
		break;
	}
	case WM_MOUSELEAVE: {
		InvalidateRect(g_thingerwnd, NULL, TRUE);

		iHighlightItem = -1;
		SetDlgItemText(hWndThinger, IDC_STATUS, TEXT(""));
		break;
	}
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
	case WM_MOUSEWHEEL: {
		PostMessage(plugin.hwndParent, uMsg, wParam, lParam);
		break;
	}
	case WM_CLOSE: {
		// prevent closing the skinned frame from destroying this window
		return 1;
	}
	case WM_ERASEBKGND: {
		// handled in WM_PAINT
		return 1;
	}
	case WM_PAINT: {
		PAINTSTRUCT ps = { 0 };
		HBRUSH hbr;
		RECT r;
		int i;
		int x;
		HBITMAP hbm, holdbm;
		HDC hdc, hdcwnd;

		GetClientRect(hWnd, &r);

		/*  Create double-buffer */
		hdcwnd = GetDC(hWnd);
		hdc = CreateCompatibleDC(hdcwnd);
		hbm = CreateCompatibleBitmap(hdcwnd, r.right, r.bottom);
		ReleaseDC(hWnd, hdcwnd);
		holdbm = (HBITMAP)SelectObject(hdc, hbm);


		/* Paint the background */
		hbr = CreateSolidBrush(WADlg_getColor(WADLG_ITEMBG));
		FillRect(hdc, &r, hbr);

		x = Scale(2) + g_scrolloffset;
		/* Paint the icons */
		for (i=0; i<IconList_GetSize(); i++) {
			lpNxSThingerIconStruct lpntis = IconList_Get(i);
			if (lpntis && ((lpntis->dwFlags & NTIS_HIDDEN) != NTIS_HIDDEN)) {
				// Determine draw method
				if ((lpntis->dwFlags & NTIS_BITMAP) == NTIS_BITMAP) {

#ifdef USE_TRANSPARENTBLT
					HDC icon_hdc;
					HBITMAP icon_hbm, icon_oldhbm;
					BITMAP bm = {0};

					icon_hbm = iHighlightItem==i ? lpntis->hBitmapHighlight : lpntis->hBitmap;
						
					// Create a DC and select the bitmap into the DC
					icon_hdc = CreateCompatibleDC(hdc);
					icon_oldhbm = (HBITMAP)SelectObject(icon_hdc, icon_hbm);

					// Get bitmap dimensions
					GetObject(icon_hbm, sizeof(BITMAP), (LPSTR)&bm);

					// Call the TransparentBlt GDI function
					GdiTransparentBlt(hdc, x, 0, Scale(0), Scale(1), icon_hdc, 0,
										0, bm.bmWidth, bm.bmHeight, 0x00FF00FF);

					// Clean up
					SelectObject(icon_hdc, icon_oldhbm);
					DeleteDC(icon_hdc);
#else
					// Use an old "True Mask method" function
					DrawTransparentBitmap(hdc,
						iHighlightItem==i ? lpntis->hBitmapHighlight : lpntis->hBitmap,
						x, 0, 0x00FF00FF);
#endif
				} else {

					DrawIconEx(hdc, x, 0, (iHighlightItem==i) ? lpntis->hIconHighlight :
								lpntis->hIcon, Scale(0), Scale(1), 0, hbr, 0);

				}
				x += Scale(0);
			}
		}

		DeleteObject(hbr);

		hdcwnd = BeginPaint(hWnd, &ps);

		// Copy double-buffer to screen
		if (buttonsDown && config_dimmedonscroll) {
			BLENDFUNCTION bf={0,0,128,0};
			GdiAlphaBlend(hdcwnd, r.left, r.top, r.right, r.bottom,
							hdc, 0, 0, r.right, r.bottom, bf);
		} else {
			BitBlt(hdcwnd, r.left, r.top, r.right, r.bottom, hdc, 0, 0, SRCCOPY);
		}

		EndPaint(hWnd, &ps);

		// Destroy double-buffer
		SelectObject(hdc, holdbm);
		DeleteObject(hbm);
		DeleteDC(hdc);
		break;
	}
	case WM_TIMER: {
		if (wParam == IDC_LEFTSCROLLBTN) {
			if (g_scrolloffset < 0)
				g_scrolloffset += SCROLLAMOUNT;
			InvalidateRect(g_thingerwnd, NULL, TRUE);
		}
		if (wParam == IDC_RIGHTSCROLLBTN) {
			RECT r;
			GetClientRect(hWnd, &r);
			r.right -= (Scale(2) * 2);
			if (g_scrolloffset > r.right - (GetNumVisibleIcons()*Scale(0)))
				g_scrolloffset -= SCROLLAMOUNT;
			InvalidateRect(g_thingerwnd, NULL, TRUE);
		}
		return TRUE;
	}
	default: break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK GenWndSubclass(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
								UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	const BOOL a = WADlg_handleDialogMsgs(hwnd, uMsg, wParam, lParam);
	if (a)
	{
		return a;
	}

	switch (uMsg) {
	case WM_CONTEXTMENU: {
		int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);
		if ((x == -1) || (y == -1)) // x and y are -1 if the user invoked a shift-f10 popup menu
		{
			RECT itemRect = { 0 };
			GetWindowRect(g_thingerwnd, &itemRect);
			x = itemRect.left;
			y = itemRect.top;
		}

		if (!g_hPopupMenu) {
			g_hPopupMenu = GetSubMenu(LoadMenu(plugin.hDllInstance, TEXT("MENU1")), 0);
		}

		SetupConfigMenu(g_hPopupMenu);

		ProcessMenuResult(TrackPopup(g_hPopupMenu, TPM_RETURNCMD|TPM_NONOTIFY|
						  TPM_LEFTBUTTON, x, y, g_thingerwnd), g_thingerwnd);
		break;
	}
	case WM_USER + 0x99: {
		const int old_dsize = dsize;
		dsize = wParam;
		upscaling = lParam;
		UpdateStatusFont();
		InvalidateRect(g_thingerwnd, NULL, TRUE);
		return 0;
	}
	case WM_WINDOWPOSCHANGING: {
		if ((SWP_NOSIZE | SWP_NOMOVE) != ((SWP_NOSIZE | SWP_NOMOVE) &
			((LPWINDOWPOS)lParam)->flags) ||
			(SWP_FRAMECHANGED & ((LPWINDOWPOS)lParam)->flags))
		{
			RECT r = { 0 };
			GetWindowRect(hWndThinger, &r);

			const int width = Scale(2),
					  height = (r.bottom - r.top) - (34 * (dsize + 1)) -
							   (config_showsb ? Scale(3) : 0),
					  top = (20 * (dsize + 1)),
					  left_origin = (11 * (dsize + 1));

			SetWindowPos(GetDlgItem(hWndThinger, IDC_LEFTSCROLLBTN),
						 NULL, left_origin, top, width, height,
						 SWP_NOACTIVATE | SWP_NOZORDER);

			const int left = (left_origin + width),
					  other_left = (r.right - r.left) - (22 * (dsize + 1));
			SetWindowPos(g_thingerwnd, NULL, left, top, (other_left - left),
						 height, SWP_NOACTIVATE | SWP_NOZORDER);

			SetWindowPos(GetDlgItem(hWndThinger, IDC_RIGHTSCROLLBTN), NULL, other_left,
						 top, width, height, SWP_NOACTIVATE | SWP_NOZORDER);

			SetWindowPos(GetDlgItem(hWndThinger, IDC_STATUS), NULL, left_origin,
						 top + height, ((other_left - left) + (width * 2)),
						 (config_showsb ? Scale(3) : 0), SWP_NOACTIVATE | SWP_NOZORDER);
		}
		break;
	}
	}
	return DefSubclass(hwnd, uMsg, wParam, lParam);
}

/*
#define PLUGIN_READMEFILE TEXT("gen_thinger.html")
void OpenSyntaxHelpAndReadMe(HWND hwndParent) {
	wchar_t syntaxfile[MAX_PATH], *p;
	HANDLE hFind;
	WIN32_FIND_DATA wfd;
	GetModuleFileName(plugin.hDllInstance, syntaxfile, sizeof(syntaxfile));
	p=syntaxfile+lstrlen(syntaxfile);
	while (p >= syntaxfile && *p != L'\\') p--;
	if (++p >= syntaxfile) *p = 0;
	lstrcat(syntaxfile, PLUGIN_READMEFILE);
	
	hFind = FindFirstFile(syntaxfile, &wfd);
	if (hFind == INVALID_HANDLE_VALUE) {
		MessageBox(hwndParent,
			TEXT("Syntax help not found!\r\n"
			"Ensure ") PLUGIN_READMEFILE TEXT(" is in Winamp\\Plugins folder."),
			(wchar_t*)plugin.description, MB_ICONWARNING);
	} else {
		FindClose(hFind);
		//ExecuteURL(syntaxfile);
		ShellExecute(hwndParent, TEXT("open"), syntaxfile, NULL, NULL, SW_SHOWNORMAL);
	}
}*/

/* [ DrawTransparentBitmap ]
* written by Paul Reynolds: Paul.Reynolds@cmgroup.co.uk
* CodeGuru article: "Transparent Bitmap - True Mask Method"
* http://codeguru.earthweb.com/bitmap/CISBitmap.shtml
*/
#ifndef USE_TRANSPARENTBLT
void DrawTransparentBitmap(HDC hDC, HBITMAP hBitmap, int x, int y, COLORREF crColour)
{
	COLORREF crOldBack = SetBkColor(hDC,RGB(255,255,255));
	COLORREF crOldText = SetTextColor(hDC,RGB(0,0,0));
	HDC dcImage, dcTrans;
	HGDIOBJ hOldBitmapImage;
	HBITMAP bitmapTrans;
	BITMAP bm = {0};
	HGDIOBJ hOldBitmapTrans;
	int nWidth;
	int nHeight;
	
	// Create two memory dcs for the image and the mask
	dcImage = CreateCompatibleDC(hDC);
	dcTrans = CreateCompatibleDC(hDC);
	
	// Select the image into the appropriate dc
	hOldBitmapImage = SelectObject(dcImage,hBitmap);
	
	// Create the mask bitmap
	bitmapTrans = NULL;
	// get the image dimensions
	GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bm);
	nWidth = bm.bmWidth;
	nHeight = bm.bmHeight;
	bitmapTrans = CreateBitmap(nWidth, nHeight, 1, 1, NULL);
	
	// Select the mask bitmap into the appropriate dc
	hOldBitmapTrans = SelectObject(dcTrans,bitmapTrans);
	
	// Build mask based on transparent colour
	SetBkColor(dcImage,crColour);
	BitBlt(dcTrans, 0, 0, nWidth, nHeight, dcImage, 0, 0, SRCCOPY);
	
	// Do the work - True Mask method - cool if not actual display
	BitBlt(hDC,x, y, nWidth, nHeight, dcImage, 0, 0, SRCINVERT);
	BitBlt(hDC,x, y, nWidth, nHeight, dcTrans, 0, 0, SRCAND);
	BitBlt(hDC,x, y, nWidth, nHeight, dcImage, 0, 0, SRCINVERT);
	
	// Restore settings
	// don't delete this, since it is the bitmap
	SelectObject(dcImage,hOldBitmapImage);
	// delete bitmapTrans
	DeleteObject(SelectObject(dcTrans,hOldBitmapTrans));
	SetBkColor(hDC,crOldBack);
	SetTextColor(hDC,crOldText);
	// clean up
	DeleteDC(dcImage);
	DeleteDC(dcTrans);
}
#endif // USE_TRANSPARENTBLT

extern "C" __declspec( dllexport ) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
	return &plugin;
}

extern "C" __declspec(dllexport) int winampUninstallPlugin(HINSTANCE hDllInst, HWND hwndDlg, int param) {
	// prompt to remove our settings with default as no (just incase)
	if (MessageBox(hwndDlg, WASABI_API_LNGSTRINGW(IDS_DO_YOU_ALSO_WANT_TO_REMOVE_SETTINGS),
		(LPWSTR)plugin.description, MB_YESNO | MB_DEFBUTTON2) == IDYES)
	{
		SaveNativeIniString(WINAMP_INI, PLUGIN_INISECTION, 0, 0);
		no_uninstall = 0;
	}

	// as we're doing too much in subclasses, etc we cannot allow for on-the-fly removal so need to do a normal reboot
	return GEN_PLUGIN_UNINSTALL_REBOOT;
}