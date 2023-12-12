
#include "stdafx.h"
#include "recaps.h"
#include "resource.h"
#include "configuration.h"
#include "trayicon.h"
#include "fixlayouts.h"
#include "settings.h"


///////////////////////////////////////////////////////////////////////////////
// Load currently active keyboard layouts from the registry
bool RegLoadBitmapData(HKEY hkey, bool bMask, HBITMAP* phBitmap)
{
	DWORD dwType = REG_BINARY, dwSize = 0;
	LPCTSTR lpcszName = bMask ? _T("FlagMask") : _T("FlagColors");
	LONG result = RegQueryValueEx(hkey, lpcszName, 0, &dwType, NULL, &dwSize);
	if (0 != result || dwSize == 0 || dwSize > 5120)
		return false;

	BITMAPINFO* pBmi = (BITMAPINFO*)alloca(dwSize);
	memset(pBmi, 0xDD, dwSize);
	result = RegQueryValueEx(hkey, lpcszName, 0, &dwType, (BYTE*)pBmi, &dwSize);
	if (0 != result)
		return false;

	int nColors = 0;
	if (pBmi->bmiHeader.biBitCount < 24)
		nColors = 1 << pBmi->bmiHeader.biBitCount;
	else if (pBmi->bmiHeader.biBitCount == 32)
		nColors = 3;

	HDC hDC = GetWindowDC(g_hwndMessageWindow);
	HBITMAP hBitmap = CreateDIBitmap(hDC, &pBmi->bmiHeader,
		CBM_INIT, &pBmi->bmiColors[nColors], pBmi, DIB_RGB_COLORS);

	if (NULL != hBitmap)
		*phBitmap = hBitmap;

	ReleaseDC(g_hwndMessageWindow, hDC);

	return true;
}


BOOL GrayScale(HBITMAP hBmp)
{
	BOOL bRet = FALSE;
	if (hBmp != NULL) {
		BITMAP bp;
		GetObject(hBmp, sizeof(BITMAP), &bp);  // bp was misspelled pb.

		long dwCount = bp.bmWidth * bp.bmHeight;
		int nBitSize = sizeof(RGBQUAD) * dwCount;
		RGBQUAD* lpBits = (RGBQUAD*)alloca(nBitSize);
		memset(lpBits, 0, nBitSize);

		GetBitmapBits(hBmp, nBitSize, lpBits);

		for (long i = 0; i < dwCount; i++) {
#if 0 
			int r = 1000 * lpBits[i].rgbRed   / 152; // 299;
			int g = 1000 * lpBits[i].rgbGreen / 587;
			int b = 1000 * lpBits[i].rgbBlue  / 116; // 114;

			int gray = b + g + r;

			lpBits[i].rgbBlue  = 0xFF & gray;
			lpBits[i].rgbGreen = 0xFF & gray;
			lpBits[i].rgbRed   = 0xFF & gray;
#else
			int r = lpBits[i].rgbRed;
			int g = lpBits[i].rgbGreen;
			int b = lpBits[i].rgbBlue;

			int gray = (min(min(r, g), b) + 3 * max(max(r, g), b)) / 4;

			lpBits[i].rgbBlue  = 0xFF & gray;
			lpBits[i].rgbGreen = 0xFF & gray;
			lpBits[i].rgbRed   = 0xFF & gray;
#endif
		}

		bRet = SetBitmapBits(hBmp, nBitSize, lpBits) > 0;
	}

	return bRet;
}


bool RegLoadIconData(HKEY hkey, HICON* phIconColor, HICON* /*phIconGray*/)
{
	ICONINFO iconInfo = { 0 };
	iconInfo.fIcon = 1;

	bool bResult = false;

	do {
		if (!RegLoadBitmapData(hkey, false, &iconInfo.hbmColor)
				|| !RegLoadBitmapData(hkey, true, &iconInfo.hbmMask))
			break;

		HICON hIcon = NULL;
		if (true == (bResult = (NULL != (hIcon = CreateIconIndirect(&iconInfo)))))
			*phIconColor = hIcon;

	} while (false);

	DeleteObject(iconInfo.hbmColor);
	DeleteObject(iconInfo.hbmMask);

	return bResult;
}


static struct {
	const TCHAR* szName;
	UHK	uhk;
} st_aHotKeys[] = {
	{ _T("Cycle"),	UHK(VK_CAPITAL, ( unsigned char )MapVirtualKey(VK_CAPITAL, MAPVK_VK_TO_VSC), 0, true, false) },
	{ _T("Recode"),	UHK(VK_CAPITAL, ( unsigned char )MapVirtualKey(VK_CAPITAL, MAPVK_VK_TO_VSC), LEFT_LCTRL, true, false) },
	{ _T("Eject"),	UHK(0, 0, 0, true, false) },
	// { _T("Caps"),		UHK(VK_CAPITAL, MapVirtualKey(VK_CAPITAL, MAPVK_VK_TO_VSC), LEFT_LALT,	true) }
};


static void LoadLegacyHotkeys(HKEY hRootKey,
							std::map<UHK, std::map<HKL, UHK>>& hotkeys)
{
	DWORD dwVK = 0; //, dwSK = 0;
	DWORD dwSize = sizeof(DWORD);
	if (ERROR_SUCCESS == RegQueryValueEx(hRootKey,
				_T("SwitchVK"), 0, NULL, (BYTE*)&dwVK, &dwSize))
	{
		st_aHotKeys[0].uhk.bits.btVK = dwVK;
		st_aHotKeys[0].uhk.bits.btSK = MapVirtualKey(dwVK, MAPVK_VK_TO_VSC);
		st_aHotKeys[1].uhk.bits.btVK = st_aHotKeys[0].uhk.bits.btVK;
		st_aHotKeys[1].uhk.bits.btSK = st_aHotKeys[0].uhk.bits.btSK;
	}

#if 0
	dwSize = sizeof(DWORD);
	if (ERROR_SUCCESS == RegQueryValueEx(hRootKey,
					_T("EjectSK"), 0, NULL, (BYTE*)&dwSK, &dwSize)
				&& ERROR_SUCCESS == RegQueryValueEx(hRootKey,
					_T("EjectVK"), 0, NULL, (BYTE*)&dwVK, &dwSize))
	{
		st_aHotKeys[2].uhk.btSK = dwSK;
		st_aHotKeys[2].uhk.btVK = dwVK;
	}
#endif

	for (int i = 0; i < _countof(st_aHotKeys); i++)
		hotkeys.insert(std::pair<UHK,
			std::map<HKL, UHK>>(st_aHotKeys[i].uhk, std::map<HKL, UHK>()));
}


static void LoadHotkeys(HKEY hRootKey, std::map<HKL, KeyboardLayoutInfo>& info,
							std::map<UHK, std::map<HKL, UHK>>& hotkeys, DWORD version)
{
	//st_aHotKeys[0].uhk.bCycleHK	= 1;
	st_aHotKeys[0].uhk.bits.btHKType	= eHKTCycle;
	//st_aHotKeys[1].uhk.bRecodeHK	= 1;
	st_aHotKeys[1].uhk.bits.btHKType	= eHKTRecode;
	//st_aHotKeys[2].uhk.bEjectHK	= 1;
	st_aHotKeys[2].uhk.bits.btHKType	= eHKTEject;
	// st_aHotKeys[3].uhk.bCapsHK		= 1;

	if (0 == version)
		return LoadLegacyHotkeys(hRootKey, hotkeys);

	HKEY hkey = 0;
	LONG result = RegOpenKey(hRootKey, _T("keys"), &hkey);

	for (int i = 0; i < _countof(st_aHotKeys); i++) {
		DWORD dwData = 0;
		DWORD dwSize = sizeof(DWORD);
		result = RegQueryValueEx(hkey, st_aHotKeys[i].szName, 0, NULL, (BYTE*)&dwData, &dwSize);
		if (result == ERROR_SUCCESS)
			st_aHotKeys[i].uhk.ulKey = dwData;

		hotkeys.insert(std::pair<UHK, std::map<HKL, UHK>>(st_aHotKeys[i].uhk, std::map<HKL, UHK>()));
	}

	DWORD dwData = 0;
	DWORD dwSize_ = sizeof(DWORD);
	result = RegQueryValueEx(hkey, _T("Group"), 0, NULL, (BYTE*)&dwData, &dwSize_);
	if (result == ERROR_SUCCESS)
		g_uhkCurrentGroup.ulKey = dwData;

	DWORD dwCount = 0;
	RegQueryInfoKey(hkey, 0, 0, 0, &dwCount, 0, 0, 0, 0, 0, 0, 0);

	for (DWORD j = 0; j < dwCount; j++) {
		TCHAR achKey[128];
		DWORD cbName = _countof(achKey);
		result = RegEnumKeyEx(hkey, j, achKey, &cbName, 0, 0, 0, 0);

		HKEY hKey = 0;
		result = RegOpenKey(hkey, achKey, &hKey);

		UHK uhk(_tcstoul(achKey, 0, 16));
		std::map<HKL, UHK> hkls;
		for (int k = 0; ERROR_SUCCESS == result; k++) {
			TCHAR name[9] = { 0 };
			_ultow(k, name, 10);
			LONG lData = 0; // LONG - sign bit will be propagated for HKLs
			DWORD dwSize = sizeof(lData);
			result = RegQueryValueEx(hKey, name, 0, NULL, (BYTE*)&lData, &dwSize);
			if (result != ERROR_SUCCESS)
				break;

			HKL hkl = reinterpret_cast<HKL>(
									static_cast<LONGLONG>(lData)); // LONGLONG to propagate sign bit
			std::map<HKL, KeyboardLayoutInfo>::iterator i = info.find(hkl);
			hkls.insert(std::pair<HKL, UHK>(hkl, i != info.end() ? i->second.inUse : true));
		}

		hotkeys.insert(std::pair<UHK, std::map<HKL, UHK>>(uhk, hkls));
		RegCloseKey(hKey);
	}

	RegCloseKey(hkey);
}


static void LoadRemaps(HKEY hRootKey, /*std::map<HKL, KeyboardLayoutInfo>& info,*/
							std::map<UHK, std::map<HKL, UHK>>& hotkeys)
{
	HKEY hkey = 0;
	LONG result = RegOpenKey(hRootKey, _T("maps"), &hkey);

	DWORD dwCount = 0;
	RegQueryInfoKey(hkey, 0, 0, 0, 0, 0, 0, &dwCount, 0, 0, 0, 0);

	for (DWORD i = 0; i < dwCount; i++) {
		TCHAR achKey[128];
		DWORD cbName = _countof(achKey);
		result = RegEnumValue(hkey, i, achKey, &cbName, 0, 0, 0, 0);
		UHK uhkFrom(_tcstoul(achKey, 0, 16));

		DWORD dwData = 0;
		DWORD dwSize = sizeof(DWORD);
		result = RegQueryValueEx(hkey, achKey, 0, NULL, (BYTE*)&dwData, &dwSize);
		if (result == ERROR_SUCCESS) {
			UHK uhkTo(dwData);
			std::map<HKL, UHK> remap;
			remap.insert(std::pair<HKL, UHK>(0, uhkTo));
			hotkeys.insert(std::pair<UHK, std::map<HKL, UHK>>(uhkFrom, remap));
		}
	}

	RegCloseKey(hkey);
}


static HICON LoadExternalIcon(LANGID langid)
{
	HICON hIcon = NULL;

	WIN32_FIND_DATA FindFileData = { 0 };

	TCHAR szModulePath[MAX_PATH] = { 0 };
	TCHAR szDrive[_MAX_DRIVE] = { 0 };
	TCHAR szDir[_MAX_DIR] = { 0 };
	GetModuleFileName(g_hInstance, szModulePath, _countof(szModulePath));
	_tsplitpath(szModulePath, szDrive, szDir, NULL, NULL);

	TCHAR szMask[MAX_PATH] = { 0 };
	_stprintf(szMask, _T("flags\\*%04d*"), langid);
	_tmakepath(szModulePath, szDrive, szDir, szMask, _T("ico"));

	HANDLE hFind = FindFirstFile(szModulePath, &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE) {
		_stprintf(szMask, _T("flags\\%s"), FindFileData.cFileName);
		_tmakepath(szModulePath, szDrive, szDir, szMask, NULL);
		hIcon = (HICON)LoadImage(NULL,
			szModulePath, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
		FindClose(hFind);
	}

	return hIcon;
}


void LoadConfiguration(std::map<HKL, KeyboardLayoutInfo>& info)
{
	HKEY hRootKey = 0;
	LONG result = RegOpenKey(HKEY_CURRENT_USER, L"Software\\Recaps", &hRootKey);

	DWORD version = 0;
	DWORD length_ = sizeof(version);
	result = RegQueryValueEx(
			hRootKey, _T("Version"), 0, NULL, (BYTE*)(&version), &length_);
	if (result != ERROR_SUCCESS)
		version = 0;

	size_t idxLED = info.size();
	if (0 == version) {
		length_ = sizeof(idxLED);
		result = RegQueryValueEx(
				hRootKey, _T("ScrollLED"), 0, NULL, (BYTE*)(&idxLED), &length_);
	}

	for (std::map<HKL, KeyboardLayoutInfo>::iterator
			i = info.begin();	i != info.end(); i++) {
		TCHAR name[9] = { 0 };
		_ui64tow(( unsigned __int64 )i->first, name, 16);
		HKEY hkey = 0;
		result = RegOpenKey(hRootKey, name, &hkey);
		if (result == ERROR_SUCCESS) {
			DWORD data = 0;
			DWORD length = sizeof(DWORD);
			result = RegQueryValueEx(
					hkey, _T("Active"), 0, NULL, (BYTE*)(&data), &length);
			if (result == ERROR_SUCCESS)
				i->second.inUse = (BOOL)data;

			if (version > 0) {
				length = sizeof(DWORD);
				result = RegQueryValueEx(
						hkey, _T("useLed"), 0, NULL, (BYTE*)(&data), &length);
				if (result == ERROR_SUCCESS)
					i->second.useLED = (BOOL)data;
			} else
				i->second.useLED = idxLED == i->second.index;

			length = sizeof(DWORD);
			result = RegQueryValueEx(
					hkey, _T("showIcon"), 0, NULL, (BYTE*)(&data), &length);
			if (result == ERROR_SUCCESS)
				i->second.showIcon = (BOOL)data;

			bool bOk = version > 0
				? RegLoadIconData(hkey, &i->second.iconColor, &i->second.iconGray) : false;
			RegCloseKey(hkey);
			if (!bOk)
				goto default_icons;

		} else {
default_icons:
			LANGID language = LOWORD(i->first);
			i->second.iconColor = LoadExternalIcon(language);
			if (NULL == i->second.iconColor)
				i->second.iconColor = (HICON)LoadImage(g_hInstance,
					MAKEINTRESOURCE(IDI_FLAG_XZ), IMAGE_ICON, 32, 32, LR_SHARED);
		}

		i->second.iconGray = CreateGrayIcon(i->second.iconColor);
	}

	LoadHotkeys(hRootKey, info, g_hotkeyInfo, version);
	LoadRemaps(hRootKey, g_hotkeyInfo);

	DWORD dwData = 0;
	DWORD dwSize = sizeof(DWORD);
	result = RegQueryValueEx(hRootKey, _T("Height"), 0, NULL, (BYTE*)&dwData, &dwSize);
	if (result == ERROR_SUCCESS)
		g_nHeightDelta = (int)dwData;

	dwSize = sizeof(DWORD);
	result = RegQueryValueEx(hRootKey, _T("Locales"), 0, NULL, (BYTE*)&dwData, &dwSize);
	if (result == ERROR_SUCCESS)
		g_nLocalesDelta = (int)dwData;

	dwSize = sizeof(DWORD);
	result = RegQueryValueEx(hRootKey, _T("Groups"), 0, NULL, (BYTE*)&dwData, &dwSize);
	if (result == ERROR_SUCCESS)
		g_nGroupsDelta = (int)dwData;

	if (0 == version) { // upgrade and delete legacy entries
		SaveConfiguration(info);
		RegDeleteValue(hRootKey, _T("EjectVK"));
		RegDeleteValue(hRootKey, _T("EjectSK"));
		RegDeleteValue(hRootKey, _T("SwitchVK"));
		RegDeleteValue(hRootKey, _T("ScrollLED"));
	}

	RegCloseKey(hRootKey);
}


HICON CreateGrayIcon(HICON hIcon)
{
	ICONINFO iconInfo = { 0 };
	if (!GetIconInfo(hIcon, &iconInfo))
		return NULL;

	BITMAP bmp = { 0 };
	GetObject(iconInfo.hbmColor, sizeof(bmp), &bmp);

	HDC hWindowDC = GetWindowDC(g_hwndMessageWindow);
	HDC hDC = CreateCompatibleDC(hWindowDC);
	HBITMAP hBitmap = CreateCompatibleBitmap(hWindowDC, bmp.bmWidth, bmp.bmHeight);

	HBRUSH hBrush = CreateSolidBrush(GetSysColor(COLOR_MENU));
	HBITMAP hOldBitmap = (HBITMAP)SelectObject(hDC, hBitmap);

	DrawIconEx(hDC, 0, 0, hIcon, bmp.bmWidth, bmp.bmHeight, 0, hBrush, DI_NORMAL);

	SelectObject(hDC, hOldBitmap);
	DeleteObject(hBrush);

	HBITMAP hGrayed = (HBITMAP)CopyImage(hBitmap,
		IMAGE_BITMAP, 0/*bmp.bmWidth*/, 0/*bmp.bmHeight*/, LR_DEFAULTSIZE);

	GrayScale(hGrayed);

	iconInfo.hbmColor = (HBITMAP)CopyImage(
			hGrayed, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);

	HICON hGrayIcon = CreateIconIndirect(&iconInfo);

	DeleteObject(hBitmap);
	DeleteObject(hGrayed);

	DeleteDC(hDC);
	ReleaseDC(g_hwndMessageWindow, hWindowDC);

	DeleteObject(iconInfo.hbmColor);
	DeleteObject(iconInfo.hbmMask);

	return hGrayIcon;
}


///////////////////////////////////////////////////////////////////////////////
// Saves currently active keyboard layouts to the registry
void RegSaveBitmapData(HKEY hkey, bool bMask, HBITMAP hBitmap)
{
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	HDC hDC = GetWindowDC(g_hwndMessageWindow);
	GetDIBits(hDC, hBitmap, 0, 0, NULL, &bmi, DIB_RGB_COLORS);

	int nBmiSize = sizeof(BITMAPINFOHEADER) + bmi.bmiHeader.biSizeImage;
	int nColors = 0;
	if (bmi.bmiHeader.biBitCount < 24)
		nColors = 1 << bmi.bmiHeader.biBitCount;
	else if (bmi.bmiHeader.biBitCount == 32)
		nColors = 3;
	nBmiSize += sizeof(RGBQUAD) * nColors;

	BITMAPINFO* pBmi = (BITMAPINFO*)alloca(nBmiSize);
	memset(pBmi, 0xDD, nBmiSize);
	memcpy(pBmi, &bmi, sizeof(BITMAPINFOHEADER));

	GetDIBits(hDC, hBitmap, 0, pBmi->bmiHeader.biHeight,
			&pBmi->bmiColors[nColors], pBmi, DIB_RGB_COLORS);

	ReleaseDC(g_hwndMessageWindow, hDC);

	nBmiSize = sizeof(BITMAPINFOHEADER) + pBmi->bmiHeader.biSizeImage;
	nColors = 0;
	if (pBmi->bmiHeader.biBitCount < 24)
		nColors = 1 << pBmi->bmiHeader.biBitCount;
	else if (pBmi->bmiHeader.biBitCount == 32)
		nColors = 3;
	nBmiSize += sizeof(RGBQUAD) * nColors;

	RegSetValueEx(hkey, bMask ? _T("FlagMask") : _T("FlagColors"),
		0, REG_BINARY, (const BYTE*)pBmi, nBmiSize);
}


void RegSaveIconData(HKEY hkey, HICON hIcon)
{
	ICONINFO iconInfo = { 0 };
	GetIconInfo(hIcon, &iconInfo);

	RegSaveBitmapData(hkey, false, iconInfo.hbmColor);
	RegSaveBitmapData(hkey, true, iconInfo.hbmMask);

	DeleteObject(iconInfo.hbmColor);
	DeleteObject(iconInfo.hbmMask);
}


static void SaveHotkeys(
		HKEY hRootKey, const std::map<UHK, std::map<HKL, UHK>>& hotkeys)
{
	HKEY hKey = 0;
	LONG result = RegCreateKey(hRootKey, _T("keys"), &hKey);

	DWORD dwCount = 0;
	RegQueryInfoKey(hKey, 0, 0, 0, &dwCount, 0, 0, 0, 0, 0, 0, 0);

	for (DWORD i = 0; i < dwCount; i++) {
		TCHAR achKey[128];
		DWORD cbName = _countof(achKey);
		result = RegEnumKeyEx(hKey, 0, achKey, &cbName, 0, 0, 0, 0);
		result = RegDeleteKey(hKey, achKey);
	}

	for (std::map<UHK, std::map<HKL, UHK>>::const_iterator i = hotkeys.begin();
				i != hotkeys.end(); i++) {
		if (eHKTGroup == i->first.bits.btHKType)
			continue;
/*		if (i->first.bCycleHK)
			st_aHotKeys[0].uhk = i->first;
		if (i->first.bRecodeHK)
			st_aHotKeys[1].uhk = i->first;
		if (i->first.bEjectHK)
			st_aHotKeys[2].uhk = i->first;*/
//		if (i->first.bCapsHK)
	//		st_aHotKeys[3].uhk = i->first;
		switch (i->first.bits.btHKType) {
		case eHKTCycle:		st_aHotKeys[0].uhk = i->first; break;
		case eHKTRecode:	st_aHotKeys[1].uhk = i->first; break;
		case eHKTEject:		st_aHotKeys[2].uhk = i->first; break;
		// case eHKTCap:			st_aHotKeys[3].uhk = i->first; break;
		}
	}

	for (int i = 0; i < _countof(st_aHotKeys); i++) {
		RegSetValueEx(hKey, st_aHotKeys[i].szName, 0, REG_DWORD,
			(const BYTE*)&(st_aHotKeys[i].uhk.ulKey), sizeof(DWORD));
	}

	RegSetValueEx(hKey, _T("Group"), 0, REG_DWORD,
			(const BYTE*)&(g_uhkCurrentGroup.ulKey), sizeof(DWORD));

	if (result == ERROR_SUCCESS) {
		for (std::map<UHK, std::map<HKL, UHK>>::const_iterator i = hotkeys.begin();
				i != hotkeys.end(); i++) {
			if (eHKTGroup != i->first.bits.btHKType)
				continue;

			TCHAR name[24] = { 0 };
			_ultot((UINT)i->first.ulKey, name, 16);

			HKEY hkey = 0;
			result = RegOpenKey(hKey, name, &hkey);
			if (result != ERROR_SUCCESS)
				result = RegCreateKey(hKey, name, &hkey);

			std::basic_string<TCHAR> desc;
			ExpandHotkeyName(desc, i->first);
			RegSetValueEx(hkey, NULL, 0, REG_SZ,
				(CONST BYTE*)desc.c_str(), DWORD(sizeof(TCHAR) * wcslen(desc.c_str()) + 1));

			int idx = 0;
			for (std::map<HKL, UHK>::const_iterator j = i->second.begin();
					j != i->second.end(); j++) {
				_ultot(idx++, name, 10);
				unsigned __int64 dwHklData = reinterpret_cast< unsigned __int64 >(j->first);
				RegSetValueEx(hkey, name, 0, REG_DWORD, (CONST BYTE*)(&dwHklData), sizeof(dwHklData));
			}

			RegCloseKey(hkey);
		}
	}

	RegCloseKey(hKey);
}


static void SaveRemaps(
		HKEY hRootKey, const std::map<UHK, std::map<HKL, UHK>>& hotkeys)
{
	HKEY hKey = 0;
	LONG result = RegCreateKey(hRootKey, _T("maps"), &hKey);

	DWORD dwCount = 0;
	RegQueryInfoKey(hKey, 0, 0, 0, 0, 0, 0, &dwCount, 0, 0, 0, 0);

	for (DWORD i = 0; i < dwCount; i++) {
		TCHAR achKey[128];
		DWORD cbName = _countof(achKey);
		result = RegEnumValue(hKey, 0, achKey, &cbName, 0, 0, 0, 0);
		result = RegDeleteValue(hKey, achKey);
	}

	for (std::map<UHK, std::map<HKL, UHK>>::const_iterator i = hotkeys.begin();
				i != hotkeys.end(); i++) {
		//if (0 == i->first.bRemapHK)
		if (eHKTRemap != i->first.bits.btHKType)
			continue;

		TCHAR name[24] = { 0 };
		_ultot((UINT)i->first.ulKey, name, 16);
		UHK uhk(i->second.empty()
			? 0 : i->second.begin()->second.ulKey);
				
		RegSetValueEx(hKey, name, 0, REG_DWORD,
			(const BYTE*)&(uhk.ulKey), sizeof(DWORD));
	}

	RegCloseKey(hKey);
}


void SaveConfiguration(const std::map<HKL, KeyboardLayoutInfo>& info)
{
	HKEY hRootKey = 0;
	LONG result = RegOpenKey(HKEY_CURRENT_USER, _T("Software\\Recaps"), &hRootKey);

	if (result != ERROR_SUCCESS)
		result = RegCreateKey(HKEY_CURRENT_USER, _T("Software\\Recaps"), &hRootKey);

	// Save current isUse value for each language
	if (result == ERROR_SUCCESS) {
		for (std::map<HKL, KeyboardLayoutInfo>::const_iterator i = info.begin();
				i != info.end(); i++) {
			TCHAR name[9] = { 0 };
			_ui64tow( ( unsigned __int64 )i->first, name, 16);
			HKEY hkey = 0;
			result = RegOpenKey(hRootKey, name, &hkey);
			if (result != ERROR_SUCCESS)
				result = RegCreateKey(hRootKey, name, &hkey);
			DWORD data = i->second.inUse;
			RegSetValueEx(hkey, _T("Active"),
					0, REG_DWORD, (CONST BYTE*)(&data), sizeof(DWORD));

			data = i->second.useLED;
			RegSetValueEx(hkey, _T("useLed"),
					0, REG_DWORD, (CONST BYTE*)(&data), sizeof(DWORD));

			data = i->second.showIcon;
			RegSetValueEx(hkey, _T("showIcon"),
					0, REG_DWORD, (CONST BYTE*)(&data), sizeof(DWORD));

			RegSetValueEx(hkey, NULL, 0, REG_SZ,
				(CONST BYTE*)i->second.name.c_str(), DWORD(sizeof(TCHAR) * wcslen(i->second.name.c_str()) + 1));

			if (NULL != i->second.iconColor)
				RegSaveIconData(hkey, i->second.iconColor);

			RegCloseKey(hkey);
		}
	}

	SaveHotkeys(hRootKey, g_hotkeyInfo);
	SaveRemaps(hRootKey, g_hotkeyInfo);

	RegSetValueEx(hRootKey, _T("Height"),  0, REG_DWORD, (const BYTE*)&g_nHeightDelta,  sizeof(DWORD));
	RegSetValueEx(hRootKey, _T("Locales"), 0, REG_DWORD, (const BYTE*)&g_nLocalesDelta, sizeof(DWORD));
	RegSetValueEx(hRootKey, _T("Groups"),  0, REG_DWORD, (const BYTE*)&g_nGroupsDelta,  sizeof(DWORD));

	DWORD version = 1;
	RegSetValueEx(hRootKey, _T("Version"),
					0, REG_DWORD, (CONST BYTE*)(&version), sizeof(DWORD));

	RegCloseKey(hRootKey);
}


bool RegCheckAndSetRunAtLogon(bool bToggleState)
{
	HKEY hRootKey = 0;
	LONG result = RegOpenKey(HKEY_CURRENT_USER,
		_T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), &hRootKey);
	if (result != ERROR_SUCCESS)
		return false;

	bool bIsSet = false;
	do {
		TCHAR szRegPath[MAX_PATH] = { 0 };
		DWORD dwType = REG_SZ;
		DWORD dwSize = _countof(szRegPath);
		result = RegQueryValueEx(hRootKey,
						_T("Recaps"), 0, &dwType, (BYTE*)szRegPath, &dwSize);
		if (result != ERROR_SUCCESS && !bToggleState)
			break;

		TCHAR szModulePath[MAX_PATH] = { 0 };
		GetModuleFileName(g_hInstance, szModulePath, _countof(szModulePath));

		bIsSet = 0 == _tcsnicmp(szModulePath, szRegPath, MAX_PATH);

		if (!bToggleState)
			break;

		if (!bIsSet)
			RegSetValueEx(hRootKey, _T("Recaps"), 0, REG_SZ, (const BYTE*)szModulePath,
				DWORD((1 + _tcslen(szModulePath)) * sizeof(TCHAR)));
		else
			RegDeleteValue(hRootKey, _T("Recaps"));

	} while (false);

	RegCloseKey(hRootKey);
	return bIsSet;
}

