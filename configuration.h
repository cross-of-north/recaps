#pragma once

#include "resource.h"


enum EMods {
	LEFT_LSHIFT	= 1 << 0,
	LEFT_RSHIFT	= 1 << 1,
	LEFT_LCTRL	= 1 << 2,
	LEFT_RCTRL	= 1 << 3,
	LEFT_LALT		= 1 << 4,
	LEFT_RALT		= 1 << 5,
	LEFT_LWIN		= 1 << 6,
	LEFT_RWIN		= 1 << 7,
	LEFT_MENU		= 1 << 8,

	LEFT_MAX		= 1 << 9
};


enum EHKType {
	eHKTGroup = 0,
	eHKTCycle,
	eHKTCaps,
	eHKTRecode,
	eHKTEject,
	eHKTRemap
};


union UHK {
	unsigned long ulKey;
	struct hotkeys_t {
		unsigned long ulHotKey	: 25;	// VK, SK, Mods
/*		unsigned long _bCycleHK	: 1;
		unsigned long _bCapsHK		: 1;
		unsigned long _bRecodeHK	: 1;
		unsigned long _bEjectHK	: 1;
		unsigned long _bRemapHK	: 1; */
	} hotkeys;

	struct bits_t {
		unsigned long btVK			: 8;
		unsigned long btSK			: 8;
		unsigned long btMods		: 9;
		unsigned long btHKType	: 5;	// Cycle, Caps, Eject etc ...
		unsigned long bExtKey		: 1;
		unsigned long bKeyDown	: 1;
	} bits;

	UHK(unsigned long key) : ulKey(key) {}
	UHK(unsigned char VK,
			unsigned char SK,
			unsigned short Mods, bool Down, bool Ext)
		: bits{ VK, SK, Mods, eHKTGroup, Ext, Down }
	{
	}

	bool operator<(const UHK &ob) const {
		return hotkeys.ulHotKey < ob.hotkeys.ulHotKey; }
};


struct KeyboardLayoutInfo {
	int												index;
	std::basic_string<TCHAR>	name;
	std::basic_string<TCHAR>	id;
	BOOL											inUse;
	BOOL											useLED;
	BOOL											showIcon;
	HICON											iconColor;
	HICON											iconGray;

	KeyboardLayoutInfo(int _index = -1,
		const TCHAR* _name = _T(""), const TCHAR* _id = _T(""))
			: index(_index)
			, name(_name)
			, id(_id)
			, inUse(TRUE)
			, useLED(FALSE)
			, showIcon(TRUE)
			, iconColor(NULL)
			, iconGray(NULL)
	{}
};


extern DWORD	g_dwModifiers;
extern bool		g_bCustomizingOn;
extern UHK		g_uhkLastHotkey;
extern DWORD	g_dwKeysCount;
extern UHK		g_uhkCurrentGroup;
extern int		g_nHeightDelta;
extern int		g_nLocalesDelta;
extern int		g_nGroupsDelta;
extern HICON  g_hHKIcon;

extern std::map<HKL, KeyboardLayoutInfo> g_keyboardInfo;
extern std::map<UHK, std::map<HKL, UHK>> g_hotkeyInfo;


int RunConfiguration(HWND hParentWnd);
bool LoadExternalIcon(HICON* phIcon, LPCTSTR lpszName);
void ExpandHotkeyName(std::basic_string<TCHAR>& name, UHK uhk);
DWORD VkToModBit(DWORD vkCode);
int AboutBoxDialog(HWND hParentWnd);

