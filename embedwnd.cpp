#include <windows.h>
#include <strsafe.h>
#include "embedwnd.h"
#include <winamp/wa_cup.h>
#include <loader/loader/utils.h>
#include <loader/loader/ini.h>

// internal variables
//HMENU main_menu = 0, windows_menu = 0;
int height = 0, width = 0;
BOOL visible = FALSE, old_visible = FALSE, self_update = FALSE;
RECT initial[2] = {0};

HWND CreateEmbeddedWindow(embedWindowState* embedWindow, const GUID embedWindowGUID)
{
	// this sets a GUID which can be used in a modern skin / other parts of Winamp to
	// indentify the embedded window frame such as allowing it to activated in a skin
	SET_EMBED_GUID((embedWindow), embedWindowGUID);

	// when creating the frame it is easier to use Winamp's handling to specify the
	// position of the embedded window when it is created saving addtional handling
	//
	// how you store the settings is down to you, this example uses winamp.ini for ease
	embedWindow->r.left = GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_x", 0);
	embedWindow->r.top = GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_y", 348);

	//TODO map from the old values?
	const int right = GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"wnd_right", -1),
			  bottom = GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"wnd_bottom", -1);

	if (right != -1)
	{
		embedWindow->r.right = right;
		SaveNativeIniString(PLUGIN_INI, INI_FILE_SECTION, L"wnd_right", 0);
	}
	else
	{
		embedWindow->r.right = embedWindow->r.left + GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_width", 275);
	}

	if (bottom != -1)
	{
		embedWindow->r.bottom = bottom;
		SaveNativeIniString(PLUGIN_INI, INI_FILE_SECTION, L"wnd_bottom", 0);
	}
	else
	{
		embedWindow->r.bottom = embedWindow->r.top + GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_height", 87);
	}

	CopyRect(&initial[0], &embedWindow->r);

	initial[1].top = height = GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"ff_height", height);
	initial[1].left = width = GetNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"ff_width", width);

	// specifying this will prevent the modern skin engine (gen_ff) from adding a menu entry
	// to the main right-click menu. this is useful if you want to add your own menu item so
	// you can show a keyboard accelerator (as we are doing) without a generic menu added
	embedWindow->flags |= EMBED_FLAGS_NOWINDOWMENU;

	// now we have set up the embedWindowState structure, we pass it to Winamp to create
	return plugin.createembed(embedWindow);
}

void DestroyEmbeddedWindow(embedWindowState* embedWindow)
{
	// unless we're closing as a classic skin then we'll
	// skip saving the current window position otherwise
	// we have the issue with windows being in the wrong
	// places after modern -> exit -> modern -> classic
	if (!embedWindow->wasabi_window &&
		!EqualRect(&initial[0], &embedWindow->r))
	{
		SaveNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_x", embedWindow->r.left);
		SaveNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_y", embedWindow->r.top);
		SaveNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_width", embedWindow->r.right - embedWindow->r.left);
		SaveNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"config_height", embedWindow->r.bottom - embedWindow->r.top);
	}

	if (old_visible != visible)
	{
		SaveNativeIniString(PLUGIN_INI, INI_FILE_SECTION,
							L"config_show", (visible ? L"1" : NULL));	}

	if (initial[1].top != height || initial[1].left != width)
	{
		SaveNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"ff_height", height);
		SaveNativeIniInt(PLUGIN_INI, INI_FILE_SECTION, L"ff_width", width);
	}
}

/*void AddEmbeddedWindowToMenus(const BOOL add, const UINT menuId, LPCWSTR menuString, const BOOL setVisible)
{
	// this will add a menu item to the main right-click menu
	if (add)
	{
		if (main_menu == NULL)
		{
		main_menu = GetNativeMenu((WPARAM)0);
		}

		if (main_menu != NULL)
		{
			const int prior_item = FindMenuItemByID(main_menu, WINAMP_OPTIONS_PLEDIT, NULL);
			AddItemToMenu2(main_menu, menuId, menuString, (prior_item + 1), TRUE);
			CheckMenuItem(main_menu, menuId, MF_BYCOMMAND |
						  (((setVisible == -1) ? visible : setVisible) ?
											MF_CHECKED : MF_UNCHECKED));
		}
	}
	else
	{
		DeleteMenu(main_menu, menuId, MF_BYCOMMAND);
	}

#ifdef IPC_ADJUST_OPTIONSMENUPOS
	// this will adjust the menu position (there were bugs with this api but all is fine for 5.5+)
	// cppcheck-suppress ConfigurationNotChecked
	SendMessage(plugin.hwndParent, WM_WA_IPC, (add ? 1 : -1), IPC_ADJUST_OPTIONSMENUPOS);
#endif

	// this will add a menu item to the main window views menu
	if (add)
	{
		if (windows_menu == NULL)
		{
		windows_menu = GetNativeMenu((WPARAM)4);
		}

		if (windows_menu != NULL)
		{
			const int prior_item = FindMenuItemByID(windows_menu, WINAMP_OPTIONS_PLEDIT, NULL);
			AddItemToMenu2(windows_menu, menuId, menuString, (prior_item + 1), TRUE);
			CheckMenuItem(windows_menu, menuId, MF_BYCOMMAND |
						  (((setVisible == -1) ? visible : setVisible) ?
											MF_CHECKED : MF_UNCHECKED));
		}
	}
	else
	{
		DeleteMenu(windows_menu,menuId,MF_BYCOMMAND);
	}

#ifdef IPC_ADJUST_FFWINDOWSMENUPOS
	// this will adjust the menu position (there were bugs with this api but all is fine for 5.5+)
	// cppcheck-suppress ConfigurationNotChecked
	SendMessage(plugin.hwndParent, WM_WA_IPC, (add ? 1 : -1), IPC_ADJUST_FFWINDOWSMENUPOS);
#endif
}

void UpdateEmbeddedWindowsMenu(const UINT menuId)
{
	const UINT check = (MF_BYCOMMAND | (visible ? MF_CHECKED : MF_UNCHECKED));
	if (main_menu)
	{
		CheckMenuItem(main_menu, menuId, check);
	}
	if (windows_menu)
	{
		CheckMenuItem(windows_menu, menuId, check);
	}
}

bool SetEmbeddedWindowMinimisedMode(HWND embeddedWindow, const bool minimised)
{
	if (minimised == true)
	{
		return SetProp(embeddedWindow, MINIMISED_FLAG, (HANDLE)1);
	}	
	RemoveProp(embeddedWindow, MINIMISED_FLAG);
	return true;
}

bool EmbeddedWindowIsMinimisedMode(HWND embeddedWindow)
{
	return (GetProp(embeddedWindow, MINIMISED_FLAG) != 0);
}*/

LRESULT HandleEmbeddedWindowChildMessages(HWND embedWnd, UINT menuId, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// we handle both messages so we can get the action when sent via the keyboard
	// shortcut but also copes with using the menu via Winamp's taskbar system menu
	if ((message == WM_SYSCOMMAND || message == WM_COMMAND) && LOWORD(wParam) == menuId)
	{
		self_update = TRUE;
		ShowHideEmbeddedWindow(embedWnd, !IsWindowVisible(embedWnd), FALSE);
		visible = !visible;
		UpdateEmbeddedWindowsMenu((UINT)menuId, visible);
		self_update = FALSE;
		return 1;
	}
	// this is sent to the child window of the frame when the 'close' button is clicked
	else if (message == WM_CLOSE)
	{
		ShowHideEmbeddedWindow(embedWnd, FALSE, TRUE);
		visible = 0;
		UpdateEmbeddedWindowsMenu((UINT)menuId, visible);
	}
	else if (message == WM_WINDOWPOSCHANGING)
	{
		/*
		 if extra_data[EMBED_STATE_EXTRA_REPARENTING] is set, we are being reparented by the freeform lib, so we should
		 just ignore this message because our visibility will not change once the freeform
		 takeover/restoration is complete
		*/
		const embedWindowState *state=(embedWindowState *)GetWindowLongPtr(embedWnd,GWLP_USERDATA);
		if (state && state->reparenting && !GetParent(embedWnd))
		{
			// this will reset the position of the frame when we need it to
			// usually from going classic->modern->close->start->classic
			SetWindowPos(embedWnd, 0, 0, 0, width, height,
						 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE |
						 SWP_NOSENDCHANGING | SWP_ASYNCWINDOWPOS);
		}
	}
	return 0;
}

void HandleEmbeddedWindowWinampWindowMessages(HWND embedWnd, UINT_PTR menuId, embedWindowState* embedWindow,
											  HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// these are done before the original window proceedure has been called to
	// ensure we get the correct size of the window and for checking the menu
	// item for the embedded window as applicable
	if (message == WM_SYSCOMMAND || message == WM_COMMAND)
	{
		if (LOWORD(wParam) == menuId)
		{
			self_update = TRUE;
			ShowHideEmbeddedWindow(embedWnd, !IsWindowVisible(embedWnd), FALSE);
			visible = !visible;
			UpdateEmbeddedWindowsMenu((UINT)menuId, visible);
			self_update = FALSE;
		}
		else if (LOWORD(wParam) == WINAMP_REFRESHSKIN)
		{
			if (!GetParent(embedWnd))
			{
				width = (embedWindow->r.right - embedWindow->r.left);
				height = (embedWindow->r.bottom - embedWindow->r.top);
			}
		}
	}
	else if (message == WM_WA_IPC)
	{
		if (lParam == IPC_SKIN_CHANGED_NEW)
		{
			RefreshInnerWindow(GetWindow(embedWnd, GW_CHILD));
		}
		else if ((lParam == IPC_CB_ONSHOWWND) || (lParam == IPC_CB_ONHIDEWND))
		{
			if (((HWND)wParam == embedWnd) && !self_update)
			{
				visible = (lParam == IPC_CB_ONSHOWWND);
				UpdateEmbeddedWindowsMenu((UINT)menuId, visible);
			}
		}
		else if ((lParam == IPC_IS_MINIMISED_OR_RESTORED) && !wParam)
		{
			// this is used to cope with Winamp being started minimised and will then
			// re-show the example window when Winamp is being restored to visibility
			if (EmbeddedWindowIsMinimisedMode(embedWnd, MINIMISED_FLAG))
			{
				ShowHideEmbeddedWindow(embedWnd, visible, FALSE);
				SetEmbeddedWindowMinimisedMode(embedWnd, MINIMISED_FLAG, FALSE);
			}
		}
	}
}