#include "stdafx.h"
#include "trayicon.h"


void AddTrayIcon(HWND hWnd, UINT uID,
		UINT uCallbackMsg, HICON hIcon, LPTSTR pszToolTip)
{
	NOTIFYICONDATA  nid     = {0};
	nid.cbSize              = sizeof(nid);
	nid.hWnd                = hWnd;
	nid.uID                 = uID;
	nid.uFlags              = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage    = uCallbackMsg;
	nid.hIcon               = hIcon;
	wcscpy_s(nid.szTip, sizeof(WCHAR)*64, pszToolTip);
	Shell_NotifyIcon(NIM_ADD, &nid);
}


void ModifyTrayIcon(
		HWND hWnd, UINT uID, HICON hIcon, LPCTSTR pszToolTip)
{
	NOTIFYICONDATA  nid = {0};
	nid.cbSize  = sizeof(nid);
	nid.hWnd    = hWnd;
	nid.uID     = uID;

	if (hIcon != NULL) {
		nid.hIcon = hIcon;
		nid.uFlags |= NIF_ICON;
	}

	if (pszToolTip) {
		wcscpy_s(nid.szTip, sizeof(WCHAR)*64, pszToolTip);
		nid.uFlags |= NIF_TIP;
	}

	if (hIcon != NULL || pszToolTip)
		Shell_NotifyIcon(NIM_MODIFY, &nid);
}


void RemoveTrayIcon(HWND hWnd, UINT uID)
{
	NOTIFYICONDATA nid = {0};
	nid.cbSize  = sizeof( nid );
	nid.hWnd    = hWnd;
	nid.uID     = uID;
	Shell_NotifyIcon( NIM_DELETE, &nid );
}

