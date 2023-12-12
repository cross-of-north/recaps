#pragma once

//  Prototypes
void AddTrayIcon( HWND hWnd, UINT uID, UINT uCallbackMsg, HICON hIcon, LPTSTR pszToolTip );
void RemoveTrayIcon( HWND hWnd, UINT uID);
void ModifyTrayIcon( HWND hWnd, UINT uID, HICON hIcon, LPCTSTR pszToolTip );

