// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#define OEMRESOURCE

#include <tchar.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>

#include <windowsx.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <process.h>
#include <winioctl.h>
#include <commdlg.h>
#include <commctrl.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations
// immediately before the previous line.

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' \
		name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
		processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' \
		language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' \
		name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
		processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' \
		language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' \
		name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
		processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' \
		language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' \
		name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
		processorArchitecture='*' publicKeyToken='6595b64144ccf1df' \
		language='*'\"")
#endif
#endif

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)

