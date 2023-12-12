#include "stdafx.h"
#include "resource.h"
#include "recaps.h"

#include "configuration.h"
#include "settings.h"


std::map<HKL, KeyboardLayoutInfo>		g_keyboardInfo;
std::map<UHK, std::map<HKL, UHK>>	g_hotkeyInfo;

UHK		g_uhkCurrentGroup	= 0;
UHK		g_uhkLastHotkey		= 0;
DWORD	g_dwKeysCount			= 0;
DWORD	g_dwModifiers			= 0;
int		g_nHeightDelta		= 0;
int		g_nLocalesDelta		= 0;
int		g_nGroupsDelta		= 0;
bool	g_bCustomizingOn	= false;
HICON g_hHKIcon					= NULL;


////////////////////////////////////////////////////////////////////////////
//
//  HotKey configuration dialog
//

static HBRUSH	st_hbrBack = 0;
static UHK		st_uhkCustHotkeyOrg = 0;
static UHK		st_uhkCustHotkey = 0;
static DWORD	st_dwPrevKeysCount = 0;


struct HotkeyDialogInit {
	UHK						uhk;
	const	TCHAR*	szCaption;
	const	TCHAR*	szDisableCaption;

	HotkeyDialogInit(UHK _uhk, const TCHAR* sz, const TCHAR* szDis = NULL)
		: uhk(_uhk), szCaption(sz), szDisableCaption(szDis) {}
};


DWORD VkToModBit(DWORD vkCode)
{
	switch (vkCode) {
	case VK_LSHIFT:  	return LEFT_LSHIFT;
	case VK_RSHIFT:  	return LEFT_RSHIFT;
	case VK_LCONTROL:	return LEFT_LCTRL;
	case VK_RCONTROL:	return LEFT_RCTRL;
	case VK_LMENU:   	return LEFT_LALT;
	case VK_RMENU:   	return LEFT_RALT;
	case VK_LWIN:			return LEFT_LWIN;
	case VK_RWIN:			return LEFT_RWIN;
	case VK_APPS:			return LEFT_MENU;
	default:
		break;
	}
	return 0;
}


void ExpandHotkeyName(std::basic_string<TCHAR>& name, UHK uhk)
{
	if (0 == uhk.hotkeys.ulHotKey) {
		name = _T("Disabled");
		return;
	}

	const TCHAR* aszModifiers[] = {
		_T("L.Shift"),		// LEFT_LSHIFT	= 1 << 0
		_T("R.Shift"),		// LEFT_RSHIFT	= 1 << 1
		_T("L.Control"),	// LEFT_LCTRL		= 1 << 2
		_T("R.Control"),	// LEFT_RCTRL		= 1 << 3
		_T("L.Alt"),			// LEFT_LALT		= 1 << 4
		_T("R.Alt"),			// LEFT_RALT		= 1 << 5
		_T("L.Win"),			// LEFT_LWIN		= 1 << 6
		_T("R.Win"),			// LEFT_RWIN		= 1 << 7
		_T("Menu")				// LEFT_MENU		= 1 << 8
	};

	DWORD dwModifiers = uhk.bits.btMods;

	dwModifiers &= ~VkToModBit(uhk.bits.btVK);

	int i = 0;
	for (DWORD mask = 1; mask != LEFT_MAX; mask <<= 1, i++)
		if (0 != (dwModifiers & mask)) {
			name += aszModifiers[i];
			name += _T(" + ");
		}

	int index = -1;
	switch (uhk.bits.btVK) {
	case VK_LSHIFT:  	index = 0; break;
	case VK_RSHIFT:  	index = 1; break;
	case VK_LCONTROL:	index = 2; break;
	case VK_RCONTROL:	index = 3; break;
	case VK_LMENU:   	index = 4; break;
	case VK_RMENU:   	index = 5; break;
	case VK_LWIN:			index = 6; break;
	case VK_RWIN:			index = 7; break;
	case VK_APPS:			index = 8; break;
	default:
		break;
	}

	if (-1 == index) {
		std::vector<TCHAR> buffer(64);
		DWORD dwScanCode = 0 == uhk.bits.btSK
			? MapVirtualKey(uhk.bits.btVK, MAPVK_VK_TO_VSC) : uhk.bits.btSK;
		if (GetKeyNameText(dwScanCode << 16 | uhk.bits.bExtKey << 24, &buffer[0], ( int )buffer.size())) {
			name += &buffer[0];
		} else {
			_sntprintf(&buffer[0], 64, _T("GetKeyNameText error %d"), GetLastError());
			OutputDebugString(&buffer[0]);
			_sntprintf(&buffer[0], 64, _T("VK:%#x; SK:%#x"), uhk.bits.btVK, dwScanCode);
			name += &buffer[0];
		}
	} else
		name += aszModifiers[index];
}


static BOOL OnInitShortCutDialog(HWND hwnd, HWND /*hwndFocus*/, LPARAM lParam)
{
	SetTimer(hwnd, 1, 100, NULL);
	st_hbrBack = CreateSolidBrush(RGB(0, 255, 0));

	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hHKIcon);

	const HotkeyDialogInit* lpInit = (const HotkeyDialogInit*)lParam;

	if (NULL != lpInit && NULL != lpInit->szCaption)
		SetWindowText(hwnd, lpInit->szCaption);

	if (NULL != lpInit && NULL != lpInit->szDisableCaption)
		SetWindowText(GetDlgItem(hwnd, IDC_DISABLE), lpInit->szDisableCaption);

	st_uhkCustHotkeyOrg = 
	st_uhkCustHotkey = lpInit->uhk;

	std::basic_string<TCHAR> hotkeyName;
	ExpandHotkeyName(hotkeyName, st_uhkCustHotkey);

	SetWindowText(GetDlgItem(hwnd, IDC_HOTKEY_STATIC), hotkeyName.c_str());

	g_bCustomizingOn = true;
	return TRUE;
}


static bool OnApply(bool bDisable)
{
	if (!bDisable) {
		std::map<UHK, std::map<HKL, UHK>>::iterator
			i = g_hotkeyInfo.find(st_uhkCustHotkey);
		if (st_uhkCustHotkeyOrg.hotkeys.ulHotKey != st_uhkCustHotkey.hotkeys.ulHotKey
					&& i != g_hotkeyInfo.end()) {
			MessageBox(NULL, _T("The hotkey is already in use. Please choose another one."),
				_T("Recaps"), MB_OK | MB_ICONWARNING | MB_TASKMODAL | MB_TOPMOST);
			return false;
		}

		switch (st_uhkCustHotkey.bits.btVK) {
		case VK_LSHIFT:
		case VK_RSHIFT:
		case VK_LCONTROL:
		case VK_RCONTROL:
		case VK_LMENU:
		case VK_RMENU:
		case VK_LWIN:
		case VK_RWIN:
		case VK_APPS:
			st_uhkCustHotkey.bits.bKeyDown = 0;
			break;
		default:
			st_uhkCustHotkey.bits.bKeyDown = 1;
			break;
		}

	} else
		st_uhkCustHotkey.hotkeys.ulHotKey = 0;

#if 1
	TCHAR ch[100] = { 0 };
	_stprintf(ch, _T("Set Hotkey %08x\n"), st_uhkCustHotkey.ulKey);
	OutputDebugString(ch);
#endif

	return true;
}


static void OnShortCutCommand(
		HWND hwnd, int id, HWND /*hwndCtl*/, UINT /*codeNotify*/)
{
	bool bDisable = false;
	switch (id) {
	case IDC_DISABLE:
		bDisable = true;
		// no break;
	case IDOK:
		if (OnApply(bDisable))
			EndDialog(hwnd, IDOK);
		break;

	case IDCANCEL:
		EndDialog(hwnd, IDCANCEL);
		break;
	}
}


static void OnShortCutTimer(HWND hwnd, UINT /*id*/)
{
	static DWORD st_dwPrevModifiers = g_dwModifiers;
	DWORD dwChanged = st_dwPrevModifiers ^ g_dwModifiers;
	st_dwPrevModifiers = g_dwModifiers;

	if (dwChanged) {
		UINT idControl = IDC_LEFT_SHIFT_STATIC;
		for (DWORD mask = 1; mask != LEFT_MAX; mask <<= 1, idControl++)
			if (0 != (dwChanged & mask))
				InvalidateRect(GetDlgItem(hwnd, idControl), NULL, FALSE);
	}

	if (st_dwPrevKeysCount != g_dwKeysCount) {
/*#ifdef _DEBUG
		TCHAR ch[100] = { 0 };
		_stprintf(ch, _T("%08x => %08x\n"), g_adwPrevHotkey[4], g_uhkLastHotkey);
		OutputDebugString(ch);
#endif */

	// copy but preserve HK flags
		st_uhkCustHotkey.bits.bKeyDown = g_uhkLastHotkey.bits.bKeyDown;
		st_uhkCustHotkey.bits.bExtKey = g_uhkLastHotkey.bits.bExtKey;
		st_uhkCustHotkey.hotkeys.ulHotKey = g_uhkLastHotkey.hotkeys.ulHotKey;

		std::basic_string<TCHAR> hotkeyName;
		ExpandHotkeyName(hotkeyName, st_uhkCustHotkey);
		SetWindowText(GetDlgItem(hwnd, IDC_HOTKEY_STATIC), hotkeyName.c_str());
		InvalidateRect(GetDlgItem(hwnd, IDC_HOTKEY_STATIC), NULL, FALSE);
	}

	st_dwPrevKeysCount = g_dwKeysCount;
}


static void OnShortCutClose(HWND hwnd)
{
	DeleteObject(st_hbrBack);
	KillTimer(hwnd, 1);
}


HBRUSH OnShortCutColorStatic(HWND hwnd, HDC hdc, HWND hwndChild, int /*type*/)
{
	UINT idControl = GetDlgCtrlID(hwndChild);

	bool bHilite = false;

	if (IDC_HOTKEY_STATIC == idControl) {
		bHilite = !g_bCustomizingOn;

	} else if (!(idControl < IDC_LEFT_SHIFT_STATIC
			|| idControl > IDC_MENU_STATIC))
		bHilite = 0 != (g_dwModifiers & (1 << (idControl - IDC_LEFT_SHIFT_STATIC)));

	if (bHilite) {
		SetBkMode(hdc, TRANSPARENT);
		return st_hbrBack;
	}

	return FORWARD_WM_CTLCOLORMSGBOX(hwnd, hdc, hwndChild, DefDlgProc);
}


static INT_PTR __stdcall ShortCutDialogProc(HWND hDlg, UINT message,
															WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		HANDLE_MSG(hDlg, WM_INITDIALOG, OnInitShortCutDialog);
		HANDLE_MSG(hDlg, WM_COMMAND, OnShortCutCommand);
		HANDLE_MSG(hDlg, WM_TIMER, OnShortCutTimer);
		HANDLE_MSG(hDlg, WM_CLOSE, OnShortCutClose);
		HANDLE_MSG(hDlg, WM_CTLCOLORSTATIC, OnShortCutColorStatic);
		default: break;
	}
	return FALSE;
}


////////////////////////////////////////////////////////////////////////////
//
//  Settings dialog
//

/* LRESULT Cls_OnSizing(HWND hwnd, UINT edge, const LPRECT lprc) */
#define HANDLE_WM_SIZING(hwnd, wParam, lParam, fn) \
	((fn)((hwnd), (UINT)(wParam), (LPRECT)(lParam)))
#define FORWARD_WM_SIZING(hwnd, edge, lprc, fn) \
	(void)(fn)((hwnd), WM_SIZING, (WPARAM)(UINT)(edge), (LPARAM)(LPRECT)(lprc))

enum {
	eHotkeysList = 0,
	eLocalesList,
	eGroupsList,
	eHintStatic,
};

enum {
	HTSPLIT_TOP = 1000,
	HTSPLIT_BOTTOM,
};

static struct {
	UINT uiId;
	HWND hwnd;
	RECT rc;
} st_aControls[] = {
	{ IDC_LIST_HOTKEYS,		     0 },
	{ IDC_LIST_LANGUAGES,	     0 },
	{ IDC_LIST_GROUPS,		     0 },
	{ IDC_PAYPAL_LINK,		     0 },
	{ IDCANCEL,						     0 }
};

static HIMAGELIST st_hImageList16;
static SIZE st_szInitDlgSize = { 0 };
static int  st_nInitHotkeysHeight = 0;
static int  st_nInitLocalesHeight = 0;
static int  st_nInitGroupsHeight = 0;
static UINT st_uiCapturedHitTest = 0;
static int  st_nCapturedY = 0;

const int cnDrawTextOffset = 6;


static BOOL InitHotkeysList(HWND hwnd)
{
	HWND hWndList = GetDlgItem(hwnd, IDC_LIST_HOTKEYS);

	DWORD dwStyleEx = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT
			| LVS_EX_AUTOSIZECOLUMNS | LVS_EX_BORDERSELECT;
	ListView_SetExtendedListViewStyleEx(hWndList, dwStyleEx, dwStyleEx);

	ListView_SetBkColor(hWndList, GetSysColor(COLOR_BTNFACE));
	ListView_SetTextBkColor(hWndList, GetSysColor(COLOR_BTNFACE));

	// init columns
	struct {
		const TCHAR* pszName;
		int iWidth;
	} aColumns[] = {
		{ _T("Action"),						200 },
		{ _T("Hotkey"),						220 },
	};

	LVCOLUMN lvc = { 0 };
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

	for (int i = 0; i < sizeof(aColumns) / sizeof(aColumns[0]); i++) {
		lvc.iSubItem = i;
		lvc.pszText = (TCHAR*)aColumns[i].pszName;
		lvc.cx = aColumns[i].iWidth;

		if ( i < 1 )
			lvc.fmt = LVCFMT_LEFT;  // Left-aligned column.
		else
			lvc.fmt = LVCFMT_RIGHT; // Right-aligned column.

		if (ListView_InsertColumn(hWndList, i, &lvc) == -1)
			return FALSE;
	}

	// Add items for the actions
	struct {
		const TCHAR* pszName;
		UHK uhk;
	} aActionItems[] = {
		{ _T("Cycle Switch"),			0 },
		{ _T("Convert Inplace"),	0 },
		{ _T("Eject CD"),					0 },
		{ _T("Toggle Caps Lock"),	0 },
	};

/*	aActionItems[0].uhk.bCycleHK	= 1;
	aActionItems[1].uhk.bRecodeHK	= 1;
	aActionItems[2].uhk.bEjectHK	= 1;
	aActionItems[3].uhk.bCapsHK		= 1;*/
	aActionItems[0].uhk.bits.btHKType = eHKTCycle;
	aActionItems[1].uhk.bits.btHKType = eHKTRecode;
	aActionItems[2].uhk.bits.btHKType = eHKTEject;
	aActionItems[3].uhk.bits.btHKType = eHKTCaps;

	for (std::map<UHK, std::map<HKL, UHK>>::iterator i = g_hotkeyInfo.begin();
			i != g_hotkeyInfo.end(); i++) {
/*		if (i->first.bCycleHK)
			aActionItems[0].uhk = i->first.ulKey;
		if (i->first.bRecodeHK)
			aActionItems[1].uhk = i->first.ulKey;
		if (i->first.bEjectHK)
			aActionItems[2].uhk = i->first.ulKey;
	//	if (i->first.bCapsHK)
		//	aActionItems[3].uhk = i->first.ulKey; */
		switch (i->first.bits.btHKType) {
		case eHKTCycle:		aActionItems[0].uhk = i->first.ulKey; break;
		case eHKTRecode:	aActionItems[1].uhk = i->first.ulKey; break;
		case eHKTEject:		aActionItems[2].uhk = i->first.ulKey; break;
		//case eHKTCaps:	aActionItems[3].uhk = i->first.ulKey; break;
		}
	}

	aActionItems[3].uhk = UHK(VK_CAPITAL,
		( unsigned char )MapVirtualKey(VK_CAPITAL, MAPVK_VK_TO_VSC), 0,	true, false);
//	aActionItems[3].uhk.bCapsHK = 1;
	aActionItems[3].uhk.bits.btHKType = eHKTCaps;
	if (aActionItems[0].uhk.bits.btVK == VK_CAPITAL)
		aActionItems[3].uhk.bits.btMods = LEFT_LALT;

	LVITEM lvi = { 0 };
	lvi.mask = LVIF_TEXT | LVIF_PARAM;

	// stock hotkeys
	for (int i = 0; i < _countof(aActionItems); i++) {
		lvi.mask |= LVIF_PARAM;
		lvi.iItem  = i;
		lvi.iSubItem = 0;
		lvi.pszText = (LPTSTR)aActionItems[i].pszName;
		lvi.lParam = (LPARAM)aActionItems[i].uhk.ulKey;

		if (ListView_InsertItem(hWndList, &lvi) == -1)
				return FALSE;

		lvi.mask &= ~(LVIF_PARAM);
		lvi.iSubItem = 1;
		std::basic_string<TCHAR> name;
		ExpandHotkeyName(name, aActionItems[i].uhk);
		lvi.pszText = (LPTSTR)name.c_str();

		if (ListView_SetItem(hWndList, &lvi) == -1)
				return FALSE;
	}

	// key remaps
	int idx = _countof(aActionItems);

	lvi.mask   = LVIF_TEXT | LVIF_PARAM;
	lvi.iImage = -1;

	for (std::map<UHK, std::map<HKL, UHK>>::iterator i = g_hotkeyInfo.begin();
			i != g_hotkeyInfo.end(); i++) {
//		if (!i->first.bRemapHK)
		if (eHKTRemap != i->first.bits.btHKType)
			continue;

		lvi.mask |= LVIF_PARAM;
		lvi.iItem  = idx++;
		lvi.iSubItem = 0;

		std::basic_string<TCHAR> name;
		ExpandHotkeyName(name, i->first);
		lvi.pszText = (LPTSTR)name.c_str();
		lvi.lParam = (LPARAM)i->first.ulKey;

		if (ListView_InsertItem(hWndList, &lvi) == -1)
				return FALSE;

		lvi.mask &= ~(LVIF_PARAM);
		lvi.iSubItem = 1;
		UHK uhk(i->second.empty() ? 0 : i->second.begin()->second);
		name.clear();
		ExpandHotkeyName(name, uhk);
		lvi.pszText = (LPTSTR)name.c_str();

		if (ListView_SetItem(hWndList, &lvi) == -1)
				return FALSE;
	}

#if 0 // disable remaps
	// <new remap> entry
	UHK uhk(0);
//	uhk.bRemapHK = 1;
	uhk.btHKType = eHKTRemap;

	lvi.mask = LVIF_TEXT | LVIF_PARAM;
	lvi.iItem  = idx;
	lvi.iSubItem = 0;
	lvi.pszText = (LPTSTR)_T("<new remap>");
	lvi.lParam = (LPARAM)uhk.ulKey;

	if (ListView_InsertItem(hWndList, &lvi) == -1)
		return FALSE;
#endif

	return TRUE;
}


static BOOL InitLanguagesList(HWND hwnd)
{
	HWND hWndList = GetDlgItem(hwnd, IDC_LIST_LANGUAGES);

	DWORD dwStyleEx = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT
			| LVS_EX_AUTOSIZECOLUMNS | LVS_EX_BORDERSELECT;
	ListView_SetExtendedListViewStyleEx(hWndList, dwStyleEx, dwStyleEx);

	ListView_SetBkColor(hWndList, GetSysColor(COLOR_BTNFACE));
	ListView_SetTextBkColor(hWndList, GetSysColor(COLOR_BTNFACE));

	// init columns
	struct {
		const TCHAR* pszName;
		int iWidth;
	} aColumns[] = {
		{ _T("+"),							20  },
		{ _T("Flag"),						50  },
		{ _T("Input Language"),	200 },
		{ _T("Scroll LED"),			70  },
		{ _T("Overlay hint"),		100 }
	};

	LVCOLUMN lvc = { 0 };
	lvc.mask = LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

	for (int i = 0; i < sizeof(aColumns) / sizeof(aColumns[0]); i++) {
		lvc.iSubItem = i;
		lvc.pszText = (TCHAR*)aColumns[i].pszName;
		lvc.cx = aColumns[i].iWidth;

		if (ListView_InsertColumn(hWndList, i, &lvc) == -1)
			return FALSE;
	}

	// Add items for the languages
	LVITEM lvi = { 0 };

	for (std::map<HKL, KeyboardLayoutInfo>::iterator i = g_keyboardInfo.begin();
			i != g_keyboardInfo.end(); i++) {
		lvi.mask = LVIF_PARAM;
		lvi.iItem  = ( int )std::distance(g_keyboardInfo.begin(), i);
		lvi.iSubItem = 0;
		lvi.lParam = (LPARAM)i->first;

		if (ListView_InsertItem(hWndList, &lvi) == -1)
				return FALSE;

		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 2;
		lvi.iImage = 0;
		lvi.pszText = (LPTSTR)i->second.name.c_str();

		if (ListView_SetItem(hWndList, &lvi) == -1)
				return FALSE;
	}
	return FALSE;
}


static BOOL InitGroupsList(HWND hwnd)
{
	HWND hWndList = GetDlgItem(hwnd, IDC_LIST_GROUPS);

	DWORD dwStyleEx = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT
			| LVS_EX_AUTOSIZECOLUMNS | LVS_EX_BORDERSELECT;
	ListView_SetExtendedListViewStyleEx(hWndList, dwStyleEx, dwStyleEx);

	ListView_SetBkColor(hWndList, GetSysColor(COLOR_BTNFACE));
	ListView_SetTextBkColor(hWndList, GetSysColor(COLOR_BTNFACE));

	// Prepare imagelist
	//HWND hWndHeader = 
	ListView_GetHeader(hWndList);

	// init columns
	LVCOLUMN lvc = { 0 };
	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

//  lvc.iSubItem = i;
	lvc.pszText = (TCHAR*)_T("Group Hotkey");
	lvc.cx = 150;

	if (ListView_InsertColumn(hWndList, 0, &lvc) == -1)
		return FALSE;

	int nWidth = (420 - 150) / ( int )g_keyboardInfo.size();
	if (nWidth < 50)
		nWidth = 50;

	for (std::map<HKL, KeyboardLayoutInfo>::iterator i = g_keyboardInfo.begin();
			i != g_keyboardInfo.end(); i++) {
		lvc.iSubItem = 1 + ( int )std::distance(g_keyboardInfo.begin(), i);
		std::basic_string<TCHAR> str(i->second.id);
		//str += _T(" *");
		lvc.pszText = (TCHAR*)str.c_str();
		lvc.cx = nWidth;
		lvc.fmt = LVCFMT_CENTER; // Right-aligned column.
		if (ListView_InsertColumn(hWndList,
					1 + std::distance(g_keyboardInfo.begin(), i), &lvc) == -1)
			return FALSE;
	}

	// Add items for hotkeys
	LVITEM lvi = { 0 };

	// Initialize LVITEM members that are common to all items.
	lvi.mask   = LVIF_TEXT | LVIF_PARAM;
	lvi.iImage = -1;

	for (std::map<UHK, std::map<HKL, UHK>>::iterator i = g_hotkeyInfo.begin();
			i != g_hotkeyInfo.end(); i++) {

		if (eHKTGroup != i->first.bits.btHKType)
			continue;

		lvi.iItem  = ( int )std::distance(g_hotkeyInfo.begin(), i);
		lvi.iSubItem = 0;

		std::basic_string<TCHAR> name;
		ExpandHotkeyName(name, i->first);
		lvi.pszText = (LPTSTR)name.c_str();
		lvi.lParam = (LPARAM)i->first.ulKey;

		if (ListView_InsertItem(hWndList, &lvi) == -1)
				return FALSE;
	}

	lvi.iItem  = ( int )g_hotkeyInfo.size();
	lvi.iSubItem = 0;
	lvi.iImage = ( int )g_hotkeyInfo.size();

	lvi.pszText = (LPTSTR)_T("<add new group>");
	lvi.lParam = (LPARAM)0;

	if (ListView_InsertItem(hWndList, &lvi) == -1)
		return FALSE;

	return FALSE;
}


static void ReposControls(HWND hwnd,
	int nHeightDelta, int nLocalesDelta, int nGroupsDelta)
{
	const int cnCount = sizeof(st_aControls) /  sizeof(*st_aControls);

	for (int i = 0; i < cnCount; i++) {
		GetWindowRect(st_aControls[i].hwnd, &st_aControls[i].rc);
		MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&st_aControls[i].rc, 2);
	}

	int nDelta = nHeightDelta - nLocalesDelta - nGroupsDelta;
	st_aControls[eHotkeysList].rc.bottom += nDelta;
	st_aControls[eLocalesList].rc.top += nDelta;
	nDelta += nLocalesDelta;

	st_aControls[eLocalesList].rc.bottom += nDelta;
	st_aControls[eGroupsList].rc.top += nDelta;
	nDelta += nGroupsDelta;

	st_aControls[eGroupsList].rc.bottom += nDelta;

  for (int i = eHintStatic; i < cnCount; i++) {
		st_aControls[i].rc.top += nDelta;
		st_aControls[i].rc.bottom += nDelta;
	}

	for (int i = 0; i < cnCount; i++) {
		SetWindowPos(st_aControls[i].hwnd, NULL,
			st_aControls[i].rc.left, st_aControls[i].rc.top,
			st_aControls[i].rc.right - st_aControls[i].rc.left,
			st_aControls[i].rc.bottom - st_aControls[i].rc.top, SWP_NOZORDER);
		InvalidateRect(st_aControls[i].hwnd, NULL, TRUE);
	}
}


static BOOL OnInitConfigureDialog(HWND hwnd, HWND /*hwndFocus*/, LPARAM /*lParam*/)
{
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIcon);

	st_hImageList16 = ImageList_Create(16, 16, ILC_MASK, ( int )g_keyboardInfo.size(), 0);
	for (std::map<HKL, KeyboardLayoutInfo>::iterator i = g_keyboardInfo.begin();
			i != g_keyboardInfo.end(); i++)
		ImageList_AddIcon(st_hImageList16,
				i->second.inUse ? i->second.iconColor : i->second.iconGray);

	ImageList_AddIcon(st_hImageList16,
			LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_CHECK)));

	InitLanguagesList(hwnd);
	InitHotkeysList(hwnd);
	InitGroupsList(hwnd);

	RECT rc;
	GetWindowRect(hwnd, &rc);
	st_szInitDlgSize.cx = rc.right - rc.left;
	st_szInitDlgSize.cy = rc.bottom - rc.top;

	SetWindowPos(hwnd, NULL, rc.left, rc.top - g_nHeightDelta / 2,
		rc.right - rc.left, rc.bottom - rc.top + g_nHeightDelta, SWP_NOZORDER);

	for (int i = 0; i < sizeof(st_aControls) / sizeof(*st_aControls); i++) {
		st_aControls[i].hwnd = GetDlgItem(hwnd, st_aControls[i].uiId);
		GetWindowRect(st_aControls[i].hwnd, &st_aControls[i].rc);
	}

	st_nInitHotkeysHeight = st_aControls[eHotkeysList].rc.bottom
														- st_aControls[eHotkeysList].rc.top;
	st_nInitLocalesHeight = st_aControls[eLocalesList].rc.bottom
														- st_aControls[eLocalesList].rc.top;
  st_nInitGroupsHeight = st_aControls[eGroupsList].rc.bottom
														- st_aControls[eGroupsList].rc.top;

	ReposControls(hwnd, g_nHeightDelta, g_nLocalesDelta, g_nGroupsDelta);

	return TRUE;
}


static LRESULT OnHotkeysClick(HWND hwnd, NMHDR* pnmhdr)
{
	do {

		LPNMITEMACTIVATE lpnma = (LPNMITEMACTIVATE)pnmhdr;
		LVHITTESTINFO lvhti = { 0 };
		lvhti.pt = lpnma->ptAction;
		if (-1 == ListView_SubItemHitTest(pnmhdr->hwndFrom, &lvhti))
			break;

		LVITEM lvi   = { 0 };
		lvi.mask     = LVIF_PARAM | LVIF_TEXT;
		lvi.iItem    = lvhti.iItem;
		std::vector<TCHAR> text(128);
		lvi.pszText = &text[0];
		lvi.cchTextMax = ( int )text.size();
		ListView_GetItem(pnmhdr->hwndFrom, &lvi);

		UHK uhk = (UHK)(( unsigned long )lvi.lParam);

//		if (1 != lvhti.iSubItem && !uhk.bRemapHK) // Name and not remapping hotkey
		if (1 != lvhti.iSubItem && eHKTRemap != uhk.bits.btHKType) // Name and not remapping hotkey
			break;

//		if (uhk.bCapsHK) { // Caps Lock hotkey is not editable.
		if (eHKTCaps == uhk.bits.btHKType) { // Caps Lock hotkey is not editable.
			MessageBox(NULL,
				_T("A 'Toggle Capslock key' hotkey is not editable.\nChange the 'Cycle Switch' one instead."),
				_T("Recaps"), MB_OK | MB_ICONINFORMATION | MB_TASKMODAL | MB_TOPMOST);
			break;
		}

		std::basic_string<TCHAR> caption(&text[0]);
		HotkeyDialogInit InitData(uhk, caption.c_str());

//		if (uhk.bRemapHK) {
		if (eHKTRemap == uhk.bits.btHKType) {
			switch (lvhti.iSubItem) {
			case 1: {
				std::map<UHK, std::map<HKL, UHK>>::iterator
					i = g_hotkeyInfo.find(uhk);
				if (i == g_hotkeyInfo.end() || i->second.empty())
					continue;
				InitData.uhk = i->second.begin()->second;
				InitData.szCaption = _T("Remap Target hotkey");
				break;
			}
			case 0:
				InitData.szCaption = _T("Remap Source hotkey");
				break;
			default:
				continue;
			}

		} else {
			caption.insert(0, _T("Action hotkey: "));
			InitData.szCaption = caption.c_str();
		}

		g_bCustomizingOn = true;
		st_dwPrevKeysCount = g_dwKeysCount;
		INT_PTR nResult = DialogBoxParam(g_hInstance,
				MAKEINTRESOURCE(IDD_SELECT_HOTKEY_DIALOG),
				hwnd, ShortCutDialogProc, (LPARAM)&InitData);
		g_bCustomizingOn = false;

		if (IDOK != nResult)
			break;

//		if (st_uhkCustHotkey.bRemapHK
		if (eHKTRemap == st_uhkCustHotkey.bits.btHKType
				&& 0 == st_uhkCustHotkey.hotkeys.ulHotKey
				&& 0 == lvhti.iSubItem) { // Deleting remaping hotkey
			std::map<UHK, std::map<HKL, UHK>>::iterator
					i = g_hotkeyInfo.find(uhk);
			if (i != g_hotkeyInfo.end())
				g_hotkeyInfo.erase(i);

			ListView_DeleteItem(pnmhdr->hwndFrom, lvhti.iItem);
			break;
		}

		// param - exclude if remap target
//		if (!(uhk.bRemapHK && 1 == lvhti.iSubItem)) {
		if (!(eHKTRemap == uhk.bits.btHKType && 1 == lvhti.iSubItem)) {
			lvi.mask = LVIF_PARAM;
			lvi.iItem  = lvhti.iItem;
			lvi.iSubItem = 0;
			lvi.lParam = (LPARAM)st_uhkCustHotkey.ulKey;
//			if (uhk.bRemapHK && 0 == uhk.ulHotKey) { // new
			if (eHKTRemap == uhk.bits.btHKType && 0 == uhk.hotkeys.ulHotKey) { // new
				ListView_InsertItem(pnmhdr->hwndFrom, &lvi);

				lvi.mask = LVIF_TEXT;
				lvi.iSubItem = 1;
				std::basic_string<TCHAR> hotkeyName;
				ExpandHotkeyName(hotkeyName, UHK(0));
				lvi.pszText = (LPTSTR)hotkeyName.c_str();
				ListView_SetItem(pnmhdr->hwndFrom, &lvi);

			} else
				ListView_SetItem(pnmhdr->hwndFrom, &lvi);
		}

		// text
		lvi.mask = LVIF_TEXT;
		lvi.iItem  = lvhti.iItem;
		lvi.iSubItem = lvhti.iSubItem;
		std::basic_string<TCHAR> hotkeyName;
		ExpandHotkeyName(hotkeyName, st_uhkCustHotkey);
		lvi.pszText = (LPTSTR)hotkeyName.c_str();
		ListView_SetItem(pnmhdr->hwndFrom, &lvi);
		//if (st_uhkCustHotkey.bCycleHK) {
		if (eHKTCycle == st_uhkCustHotkey.bits.btHKType) {
			lvi.iItem = 3;
			if (st_uhkCustHotkey.bits.btVK == VK_CAPITAL)
				lvi.pszText = (LPTSTR)_T("L.Alt + Caps Lock");
			else
				lvi.pszText = (LPTSTR)_T("Caps Lock");
			ListView_SetItem(pnmhdr->hwndFrom, &lvi);
		}

		if (st_uhkCustHotkey.hotkeys.ulHotKey != InitData.uhk.hotkeys.ulHotKey) {
			std::map<UHK, std::map<HKL, UHK>>::iterator
				i = g_hotkeyInfo.find(uhk);
//			if (uhk.bRemapHK && 1 == lvhti.iSubItem) {
			if (eHKTRemap == uhk.bits.btHKType && 1 == lvhti.iSubItem) {
				if (i != g_hotkeyInfo.end())
					if (!i->second.empty())
						i->second.begin()->second = st_uhkCustHotkey;
//			} else if (uhk.bRemapHK && 0 == uhk.ulHotKey) { // new
			} else if (eHKTRemap == uhk.bits.btHKType && 0 == uhk.hotkeys.ulHotKey) { // new
				std::map<HKL, UHK> remap;
				remap.insert(std::pair<HKL, UHK>(0, 0));
				g_hotkeyInfo.insert(
					std::pair<UHK, std::map<HKL, UHK>>(st_uhkCustHotkey, remap));
			} else {
				// write back to map
				if (i != g_hotkeyInfo.end()) {
					std::map<HKL, UHK> remap;
					if (!i->second.empty())
						remap.insert(std::pair<HKL, UHK>(0, i->second.begin()->second));
					g_hotkeyInfo.erase(i);
					g_hotkeyInfo.insert(
						std::pair<UHK, std::map<HKL, UHK>>(st_uhkCustHotkey, remap));
				}
			}
		}

	} while (false);

	return 0;
}


static LRESULT OnLanguagesClick(HWND hwnd, NMHDR* pnmhdr)
{
	LPNMITEMACTIVATE lpnma = (LPNMITEMACTIVATE)pnmhdr;
	LVHITTESTINFO lvhti = { 0 };
	lvhti.pt = lpnma->ptAction;
	if (-1 != ListView_SubItemHitTest(pnmhdr->hwndFrom, &lvhti)) {
		LVITEM lvi   = { 0 };
		lvi.mask     = LVIF_PARAM;
		lvi.iItem    = lvhti.iItem;
		ListView_GetItem(pnmhdr->hwndFrom, &lvi);

		HKL hkl = (HKL)(lvi.lParam);

		lvi.mask     = LVIF_TEXT;
		lvi.pszText  = (LPTSTR)"";
		lvi.iItem    = lvhti.iItem;
		lvi.iSubItem = lvhti.iSubItem;

		bool bUpdateIcon = false;

		switch (lvhti.iSubItem) {
		case 0: // Active
			g_keyboardInfo[hkl].inUse = !g_keyboardInfo[hkl].inUse;

			for (std::map<UHK, std::map<HKL, UHK>>::iterator
					hk_i = g_hotkeyInfo.begin(); hk_i != g_hotkeyInfo.end(); hk_i++)
				for (std::map<HKL, UHK>::iterator i = hk_i->second.begin();
						i != hk_i->second.end(); i++)
					if (hkl == i->first)
						i->second = g_keyboardInfo[hkl].inUse;

			bUpdateIcon = true;
			break;
		case 1: // Flag
			lvi.mask   = LVIF_IMAGE;
			lvi.iImage = lvhti.iItem;
			if (LoadExternalIcon(&g_keyboardInfo[hkl].iconColor,
							g_keyboardInfo[hkl].name.c_str())) {
				g_keyboardInfo[hkl].iconGray
					= CreateGrayIcon(g_keyboardInfo[hkl].iconColor);
				bUpdateIcon = true;
			}
			break;
		case 2:	// Name
			lvi.mask = 0;
			break;
		case 3: // Scroll LED
			g_keyboardInfo[hkl].useLED = !g_keyboardInfo[hkl].useLED;
			break;
		case 4: // Overlay
			g_keyboardInfo[hkl].showIcon = !g_keyboardInfo[hkl].showIcon;
			break;
		}

		if (bUpdateIcon) {
			ImageList_ReplaceIcon(st_hImageList16, lvhti.iItem,
				g_keyboardInfo[hkl].inUse
					? g_keyboardInfo[hkl].iconColor : g_keyboardInfo[hkl].iconGray);
			InvalidateRect(GetDlgItem(hwnd, IDC_LIST_GROUPS), NULL, TRUE);
		}

		ListView_SetItem(pnmhdr->hwndFrom, &lvi);
	}

	return 0;
}


static LRESULT OnGroupsClick(HWND hwnd, NMHDR* pnmhdr)
{
	LPNMITEMACTIVATE lpnma = (LPNMITEMACTIVATE)pnmhdr;
	LVHITTESTINFO lvhti = { 0 };
	lvhti.pt = lpnma->ptAction;
	if (-1 != ListView_SubItemHitTest(pnmhdr->hwndFrom, &lvhti)) {
		LVITEM lvi_   = { 0 };
		lvi_.mask     = LVIF_PARAM | LVIF_TEXT;
		std::vector<TCHAR> text(128);
		lvi_.pszText = &text[0];
		lvi_.cchTextMax = ( int )text.size();
		lvi_.iItem    = lvhti.iItem;
		ListView_GetItem(pnmhdr->hwndFrom, &lvi_);

		UHK uhk = (UHK)(( unsigned long )lvi_.lParam);
		std::basic_string<TCHAR> caption(_T("Group hotkey: "));
		caption += &text[0];

		lvi_.mask     = LVIF_TEXT;
		lvi_.pszText  = (LPTSTR)_T("");
		lvi_.iItem    = lvhti.iItem;
		lvi_.iSubItem = lvhti.iSubItem;

		if (0 == lvhti.iSubItem) { // Hotkey
			lvi_.mask = 0;
			HotkeyDialogInit InitData(uhk, caption.c_str(), _T("Delete"));
			g_bCustomizingOn = true;
			st_dwPrevKeysCount = g_dwKeysCount;

			do {

				if (IDOK != DialogBoxParam(g_hInstance,
					MAKEINTRESOURCE(IDD_SELECT_HOTKEY_DIALOG),
						hwnd, ShortCutDialogProc, (LPARAM)&InitData))
				break;

				if (st_uhkCustHotkey.hotkeys.ulHotKey == uhk.hotkeys.ulHotKey)
					break;

				if (0 == st_uhkCustHotkey.hotkeys.ulHotKey) { // Disabled
						std::map<UHK, std::map<HKL, UHK>>::iterator
							i = g_hotkeyInfo.find(uhk);
					if (i != g_hotkeyInfo.end())
						g_hotkeyInfo.erase(i);

					ListView_DeleteItem(pnmhdr->hwndFrom, lvhti.iItem);
					break;
				}

				LVITEM lvi = { 0 };
				lvi.mask = LVIF_PARAM | LVIF_TEXT;
				lvi.iItem  = lvhti.iItem;
				lvi.iSubItem = 0;
				lvi.lParam = (LPARAM)st_uhkCustHotkey.ulKey;
				std::basic_string<TCHAR> hotkeyName;
				ExpandHotkeyName(hotkeyName, st_uhkCustHotkey);
				lvi.pszText = (LPTSTR)hotkeyName.c_str();

				if (0 == uhk.ulKey) { // new
					g_hotkeyInfo.insert(
						std::pair<UHK, std::map<HKL, UHK>>(
							st_uhkCustHotkey, std::map<HKL, UHK>()));
					ListView_InsertItem(pnmhdr->hwndFrom, &lvi);
					break;
				}

				std::map<UHK, std::map<HKL, UHK>>::iterator
					i = g_hotkeyInfo.find(uhk);
				if (i != g_hotkeyInfo.end()) {
					std::pair<UHK, std::map<HKL, UHK>>
						p(st_uhkCustHotkey, i->second);
					g_hotkeyInfo.erase(i);
					g_hotkeyInfo.insert(p);
				}

				ListView_SetItem(pnmhdr->hwndFrom, &lvi);

			} while (false);

			g_bCustomizingOn = false;

		} else if (lvhti.iSubItem > 0
				&& lvhti.iSubItem <= g_keyboardInfo.size()) {
			if (0 == uhk.ulKey) // <new> line
				return 0;

			int idx = lvhti.iSubItem - 1;
			std::map<HKL, KeyboardLayoutInfo>::iterator i_ki = g_keyboardInfo.begin();
			std::advance(i_ki, idx);
			std::map<UHK, std::map<HKL, UHK>>::iterator i_hk = g_hotkeyInfo.find(uhk);

			if (i_ki != g_keyboardInfo.end() && i_hk != g_hotkeyInfo.end()) {
				std::map<HKL, UHK>::iterator i = i_hk->second.find(i_ki->first);
				if (i == i_hk->second.end())
					i_hk->second.insert(std::pair<HKL, UHK>(i_ki->first, i_ki->second.inUse));
				else
					i_hk->second.erase(i);
			}
		}

		ListView_SetItem(pnmhdr->hwndFrom, &lvi_);
	}

	return 0;
}


static LRESULT OnDonationClick(HWND hwnd, NMHDR* /*pnmhdr*/)
{
	ShellExecute(NULL, _T("open"),
		L"https://www.paypal.com/cgi-bin/webscr?cmd=_donations"
		L"&business=zharik@gmx.li&lc=BY&item_name=Just a thank for the Recaps app."
		L"&no_note=0&currency_code=USD&bn=PP-DonationsBF",
			NULL, NULL, SW_SHOWNORMAL);
	PostMessage(hwnd, WM_COMMAND, 
			MAKEWPARAM(IDOK, BN_CLICKED), LPARAM(GetDlgItem(hwnd, IDOK)));
	return 0;
}

static bool IsHotkeyEditable(int iSubItem, UHK uhk)
{
	if (0 == iSubItem)
		return eHKTRemap == uhk.bits.btHKType ? true : false;

	// sub item > 0
	if (eHKTCaps == uhk.bits.btHKType)
		return false;

	if (eHKTRemap != uhk.bits.btHKType)
		return true;
	
	// only remapped
	std::map<UHK, std::map<HKL, UHK>>::iterator i = g_hotkeyInfo.find(uhk);
	return !(i == g_hotkeyInfo.end() || i->second.empty());
}


static LRESULT OnHotkeysHotTrack(HWND /*hwnd*/, NMHDR* pnmhdr)
{
	LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pnmhdr;

	LVHITTESTINFO lvhti = { 0 };
	lvhti.pt = lpnmlv->ptAction;
	if (-1 != ListView_SubItemHitTest(pnmhdr->hwndFrom, &lvhti)) {
		LVITEM lvi   = { 0 };
		lvi.mask     = LVIF_PARAM;
		lvi.iItem    = lvhti.iItem;
		ListView_GetItem(pnmhdr->hwndFrom, &lvi);

		UHK uhk = (UHK)(( unsigned long )lvi.lParam);
		SetCursor(LoadCursor(NULL, IsHotkeyEditable(lvhti.iSubItem, uhk)
					? IDC_HAND : IDC_ARROW));
	}

	return 0;
}


static LRESULT OnLanguagesHotTrack(HWND /*hwnd*/, NMHDR* pnmhdr)
{
	LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pnmhdr;

	LVHITTESTINFO lvhti = { 0 };
	lvhti.pt = lpnmlv->ptAction;
	if (-1 != ListView_SubItemHitTest(pnmhdr->hwndFrom, &lvhti)) {
		LVITEM lvi   = { 0 };
		lvi.mask     = LVIF_PARAM;
		lvi.iItem    = lvhti.iItem;
		ListView_GetItem(pnmhdr->hwndFrom, &lvi);

		HKL hkl = (HKL)(lvi.lParam);
		bool bSetCursor = g_keyboardInfo[hkl].inUse;

		switch (lvhti.iSubItem) {
		case 0: // Active
			bSetCursor = true;
			break;
		case 1: // Flag
			break;
		case 2:	// Name
			bSetCursor = false;
			break;
		case 3: // Scroll LED
			break;
		case 4: // Overlay
			break;
		}

		if (bSetCursor)
			SetCursor(LoadCursor(NULL, IDC_HAND));
	}

	return 0;
}


static LRESULT OnGroupsHotTrack(HWND /*hwnd*/, NMHDR* pnmhdr)
{
	LPNMLISTVIEW lpnmlv = (LPNMLISTVIEW)pnmhdr;

	LVHITTESTINFO lvhti = { 0 };
	lvhti.pt = lpnmlv->ptAction;
	if (-1 != ListView_SubItemHitTest(pnmhdr->hwndFrom, &lvhti)) {
		LVITEM lvi   = { 0 };
		lvi.mask     = LVIF_PARAM;
		lvi.iItem    = lvhti.iItem;
		ListView_GetItem(pnmhdr->hwndFrom, &lvi);

		UHK uhk = (UHK)( ( unsigned long )lvi.lParam);
		if (0 == lvhti.iSubItem || 0 != uhk.ulKey)
			SetCursor(LoadCursor(NULL, IDC_HAND));
	}

	return 0;
}


/*static LRESULT OnCustomDrawLanguages(NMLVCUSTOMDRAW* pNMLVCUSTOMDRAW)
{
	do {

		if (2 == pNMLVCUSTOMDRAW->iSubItem) // name
			break;

		if (0 != (pNMLVCUSTOMDRAW->nmcd.uItemState & CDIS_SELECTED))
			FillRect(pNMLVCUSTOMDRAW->nmcd.hdc,
				&pNMLVCUSTOMDRAW->nmcd.rc, GetSysColorBrush(COLOR_HIGHLIGHT));

		HKL hkl = (HKL)pNMLVCUSTOMDRAW->nmcd.lItemlParam;

		std::map<HKL, KeyboardLayoutInfo>::iterator i = g_keyboardInfo.find(hkl);
		if (i == g_keyboardInfo.end())
			break;

		int index = -1;
		switch (pNMLVCUSTOMDRAW->iSubItem) {
		case 0: // inUse
			if (i->second.inUse)
				index = g_keyboardInfo.size(); // check
			break;
		case 1: // Flag
			index = std::distance(g_keyboardInfo.begin(), i);
			break;
		case 3: // LED
			if (i->second.inUse && i->second.useLED)
				index = g_keyboardInfo.size(); // check
			break;
		case 4: // Overlay
			if (i->second.inUse && i->second.showIcon)
				index = std::distance(g_keyboardInfo.begin(), i);
			break;
		default:
			continue; // skip to CDRF_DODEFAULT
		}

		if (index < 0)
			break;

		ImageList_Draw(st_hImageList16, index, pNMLVCUSTOMDRAW->nmcd.hdc,
			(pNMLVCUSTOMDRAW->nmcd.rc.left
				+ pNMLVCUSTOMDRAW->nmcd.rc.right) / 2 - 16 / 2,
			(pNMLVCUSTOMDRAW->nmcd.rc.top
				+ pNMLVCUSTOMDRAW->nmcd.rc.bottom) / 2 - 16 / 2,
			ILD_NORMAL);

		return CDRF_SKIPDEFAULT;

	} while (false);

	return CDRF_DODEFAULT;
}


static LRESULT OnCustomDrawGroups(NMLVCUSTOMDRAW* pNMLVCUSTOMDRAW)
{
	do {
		if (pNMLVCUSTOMDRAW->iSubItem < 1) // hotkey
			break;

		if (0 != (pNMLVCUSTOMDRAW->nmcd.uItemState & CDIS_SELECTED))
			FillRect(pNMLVCUSTOMDRAW->nmcd.hdc,
				&pNMLVCUSTOMDRAW->nmcd.rc, GetSysColorBrush(COLOR_HIGHLIGHT));

		UHK uhk(pNMLVCUSTOMDRAW->nmcd.lItemlParam);
		std::map<UHK, std::map<HKL, UHK>>::iterator i_hk = g_hotkeyInfo.find(uhk);
		if (i_hk == g_hotkeyInfo.end())
			break;

		for (std::map<HKL, UHK>::iterator
				i = i_hk->second.begin(); i != i_hk->second.end(); i++) {
			std::map<HKL, KeyboardLayoutInfo>::iterator
					i_kb = g_keyboardInfo.find(i->first);
			if (i_kb == g_keyboardInfo.end())
				continue;

			if (pNMLVCUSTOMDRAW->iSubItem
					!= (1 + std::distance(g_keyboardInfo.begin(), i_kb)))
				continue;

			ImageList_Draw(st_hImageList16,
				pNMLVCUSTOMDRAW->iSubItem - 1, pNMLVCUSTOMDRAW->nmcd.hdc,
				(pNMLVCUSTOMDRAW->nmcd.rc.left
					+ pNMLVCUSTOMDRAW->nmcd.rc.right) / 2 - 16 / 2,
				(pNMLVCUSTOMDRAW->nmcd.rc.top
					+ pNMLVCUSTOMDRAW->nmcd.rc.bottom) / 2 - 16 / 2,
				ILD_NORMAL);

			return CDRF_SKIPDEFAULT;
		}

	} while (false);

	return CDRF_DODEFAULT;
}


static LRESULT OnConfigureCustomDraw(HWND hwnd, NMHDR* pNMHDR)
{
	NMLVCUSTOMDRAW* pNMLVCUSTOMDRAW = (NMLVCUSTOMDRAW*)pNMHDR;

	bool bLanguages = false;
	switch (pNMLVCUSTOMDRAW->nmcd.hdr.idFrom) {
	case IDC_LIST_LANGUAGES:
		bLanguages = true;
		// no break
	case IDC_LIST_GROUPS:
		break;
	default:
		return 0;
	}

	LRESULT lResult = CDRF_DODEFAULT;
	switch (pNMLVCUSTOMDRAW->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
	case CDDS_ITEMPREPAINT:
		lResult = CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYSUBITEMDRAW;
		break;
	case CDDS_ITEMPREPAINT | CDDS_SUBITEM:
		lResult = bLanguages
			? OnCustomDrawLanguages(pNMLVCUSTOMDRAW)
					: OnCustomDrawGroups(pNMLVCUSTOMDRAW);
		break;
	default:
		break;
	}

	if (CDRF_DODEFAULT != lResult)
		SetDlgMsgResult(hwnd, WM_NOTIFY, lResult);

	return TRUE;
} */


static void OnConfigureCommand(
		HWND hwnd, int id, HWND /*hwndCtl*/, UINT /*codeNotify*/)
{
	switch (id) {
	case IDOK: 		 EndDialog(hwnd, IDOK);
	case IDCANCEL: EndDialog(hwnd, IDCANCEL);
	}
}


static LRESULT OnConfigureNotify(HWND hwnd, int idFrom, NMHDR* pnmhdr)
{
	if (NULL == pnmhdr)
		return 0;

	switch (pnmhdr->code) {
/*	case NM_DBLCLK:
		switch (idFrom) {
		case IDC_LIST_HOTKEYS:		return OnHotkeysDblClick(hwnd, pnmhdr);
		case IDC_LIST_LANGUAGES:	return OnLanguagesDblClick(hwnd, pnmhdr);
		case IDC_LIST_GROUPS:			return OnGroupsDblClick(hwnd, pnmhdr);
		}
		break; */
	case NM_CLICK:
		switch (idFrom) {
		case IDC_LIST_HOTKEYS:		return OnHotkeysClick(hwnd, pnmhdr);
		case IDC_LIST_LANGUAGES:	return OnLanguagesClick(hwnd, pnmhdr);
		case IDC_LIST_GROUPS:			return OnGroupsClick(hwnd, pnmhdr);
		case IDC_PAYPAL_LINK:			return OnDonationClick(hwnd, pnmhdr);
		}
		break;
/*	case NM_CUSTOMDRAW:
	case NM_OUTOFMEMORY:
		return OnConfigureCustomDraw(hwnd, pnmhdr); */
	case LVN_HOTTRACK:
		switch (idFrom) {
		case IDC_LIST_HOTKEYS:		return OnHotkeysHotTrack(hwnd, pnmhdr);
		case IDC_LIST_LANGUAGES:	return OnLanguagesHotTrack(hwnd, pnmhdr);
		case IDC_LIST_GROUPS:			return OnGroupsHotTrack(hwnd, pnmhdr);
		}
		break;
	default:
		break;
	}

	return 0;
}


static void OnConfigureOwnerDrawHotkeys(HWND /*hwnd*/, const DRAWITEMSTRUCT *lpDrawItem)
{
	RECT rcItem(lpDrawItem->rcItem);
	rcItem.right = rcItem.left;

	UHK uhk( ( unsigned long )lpDrawItem->itemData );

	LV_COLUMN lvc = { 0 };
	lvc.mask = LVCF_WIDTH;
	for (int nColumn = 0; 
			ListView_GetColumn(lpDrawItem->hwndItem, nColumn, &lvc); 
			nColumn++, rcItem.left += lvc.cx) {
		rcItem.right += lvc.cx;

		SetTextColor(lpDrawItem->hDC, IsHotkeyEditable(nColumn, uhk)
				? GetSysColor(COLOR_HOTLIGHT) : GetSysColor(COLOR_WINDOWTEXT));

		TCHAR aText[32] = { 0 };
		LVITEM lvi = { 0 };
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		lvi.iItem = lpDrawItem->itemID;
		lvi.iSubItem = nColumn;
		lvi.cchTextMax = _countof(aText);
		lvi.pszText = aText;
		ListView_GetItem(lpDrawItem->hwndItem, &lvi);

		RECT rcLabel(rcItem);
		rcLabel.left += cnDrawTextOffset;
		rcLabel.right -= cnDrawTextOffset;
		DrawText(lpDrawItem->hDC, aText, ( int )_tcslen(aText), 
			&rcLabel, (nColumn == 0 ? DT_LEFT : DT_RIGHT)
				| DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER);
	}

	SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOWTEXT));
}


static void OnConfigureOwnerDrawLanguages(HWND /*hwnd*/, const DRAWITEMSTRUCT *lpDrawItem)
{
	RECT rcItem(lpDrawItem->rcItem);
	rcItem.right = rcItem.left;

	HKL hkl = (HKL)lpDrawItem->itemData;

	std::map<HKL, KeyboardLayoutInfo>::iterator i = g_keyboardInfo.find(hkl);
	if (i == g_keyboardInfo.end())
		return;

	LV_COLUMN lvc = { 0 };
	lvc.mask = LVCF_WIDTH;
	for (int nColumn = 0; 
			ListView_GetColumn(lpDrawItem->hwndItem, nColumn, &lvc); 
			nColumn++, rcItem.left += lvc.cx) {
		rcItem.right += lvc.cx;
/*		if (ODS_SELECTED & lpDrawItem->itemState) {
			FillRect(lpDrawItem->hDC, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
			SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
		} */

		if (!i->second.inUse)
			SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_GRAYTEXT));

		int index = -1;
		switch (nColumn) {
		case 0: // inUse
			if (i->second.inUse)
				index = ( int )g_keyboardInfo.size(); // check
			break;
		case 1: // Flag
			index = ( int )std::distance(g_keyboardInfo.begin(), i);
			break;
		case 2: { // Name
			RECT rcLabel(rcItem);
			rcLabel.left += cnDrawTextOffset;
			rcLabel.right -= cnDrawTextOffset;
			DrawText(lpDrawItem->hDC, i->second.name.c_str(), ( int )i->second.name.size(),
				&rcLabel, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER); 
			continue;
		}
		case 3: // LED
			if (i->second.inUse && i->second.useLED)
				index = ( int )g_keyboardInfo.size(); // check
			break;
		case 4: // Overlay
			if (i->second.inUse && i->second.showIcon)
				index = ( int )std::distance(g_keyboardInfo.begin(), i);
			break;
		default:
			continue; // skip ?
		}

		if (index < 0)
			continue;

		ImageList_Draw(st_hImageList16, index, lpDrawItem->hDC,
			(rcItem.left + rcItem.right) / 2 - 16 / 2,
			(rcItem.top + rcItem.bottom) / 2 - 16 / 2,
			ILD_NORMAL);
	}

	SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOWTEXT));
}


static void OnConfigureOwnerDrawGroups(HWND /*hwnd*/, const DRAWITEMSTRUCT *lpDrawItem)
{
	RECT rcItem(lpDrawItem->rcItem);
	rcItem.right = rcItem.left;

	std::map<UHK, std::map<HKL, UHK>>::iterator i_hk = g_hotkeyInfo.end(); 
	std::basic_string<TCHAR> name;

	switch (lpDrawItem->itemData) {
	case 0:
		name = _T("<add new group>");
		break;
#if 0 // SYSKEY - see shelve!
	case -1: {
		name = _T("[System] ");
		switch (g_eSystemHotkey) {
		case eAltShift:  name += _T("L.Alt + Shift"); break;
		case eCtrlShift: name += _T("Ctrl + Shift");  break;
		case eAccent:		 name += _T("Grave Accent");  break;
		default:
		case eDisabled:	 name += _T("Disabled");   		break;
		}
		} break;
#endif // SYSKEY
	default: {
		UHK uhk( ( unsigned long )lpDrawItem->itemData);
		i_hk = g_hotkeyInfo.find(uhk);
		if (i_hk == g_hotkeyInfo.end())
			name += _T("Unknown");
		else
			ExpandHotkeyName(name, i_hk->first);
		}
		break;
	}

	LV_COLUMN lvc = { 0 };
	lvc.mask = LVCF_WIDTH;
	for (int nColumn = 0; 
			ListView_GetColumn(lpDrawItem->hwndItem, nColumn, &lvc); 
			nColumn++, rcItem.left += lvc.cx) {
		rcItem.right += lvc.cx;
/*		if (ODS_SELECTED & lpDrawItem->itemState) {
			FillRect(lpDrawItem->hDC, &rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
		  SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
		} */
		SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_HOTLIGHT/*COLOR_HIGHLIGHTTEXT*/));

		if (0 == nColumn) { // Name
			RECT rcLabel(rcItem);
			rcLabel.left += cnDrawTextOffset;
			rcLabel.right -= cnDrawTextOffset;
			DrawText(lpDrawItem->hDC, name.c_str(), ( int )name.size(),
				&rcLabel, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_VCENTER); 
			continue;
		}

		if (0 == lpDrawItem->itemData)
			break; // add new group item - no flag icons

		int index = nColumn - 1;
		std::map<HKL, KeyboardLayoutInfo>::iterator
					i_kb = g_keyboardInfo.begin();
		std::advance(i_kb, index);
		if (i_hk->second.find(i_kb->first) == i_hk->second.end())
			continue;

		ImageList_Draw(st_hImageList16, index, lpDrawItem->hDC,
			(rcItem.left + rcItem.right) / 2 - 16 / 2,
			(rcItem.top + rcItem.bottom) / 2 - 16 / 2,
			ILD_NORMAL);
	}

	SetTextColor(lpDrawItem->hDC, GetSysColor(COLOR_WINDOWTEXT));
}


static void OnConfigureDrawItem(HWND hwnd, const DRAWITEMSTRUCT *lpDrawItem)
{
	if (ODT_LISTVIEW != lpDrawItem->CtlType)
		return;

	switch (lpDrawItem->CtlID) {
	case IDC_LIST_HOTKEYS:
		OnConfigureOwnerDrawHotkeys(hwnd, lpDrawItem);
		break;
	case IDC_LIST_LANGUAGES:
		OnConfigureOwnerDrawLanguages(hwnd, lpDrawItem);
		break;
	case IDC_LIST_GROUPS:
		OnConfigureOwnerDrawGroups(hwnd, lpDrawItem);
		break;
	default:
		break;
	}
}


static LRESULT OnSizing(HWND hwnd, UINT edge, LPRECT lpRect)
{
	BOOL bSizeFixed = FALSE;

	switch (edge) {
	case WMSZ_LEFT:
	case WMSZ_TOPLEFT:
	case WMSZ_BOTTOMLEFT:
		lpRect->left = lpRect->right - st_szInitDlgSize.cx;
		bSizeFixed = TRUE;
		break;
	case WMSZ_RIGHT:
	case WMSZ_TOPRIGHT:
	case WMSZ_BOTTOMRIGHT:
		lpRect->right = lpRect->left + st_szInitDlgSize.cx;
		bSizeFixed = TRUE;
		break;
	case WMSZ_TOP:
	case WMSZ_BOTTOM:
		break;
	}

	switch (edge) {
	case WMSZ_LEFT:
	case WMSZ_RIGHT:
		break;
	case WMSZ_TOP:
	case WMSZ_TOPLEFT:
	case WMSZ_TOPRIGHT:
		if (lpRect->bottom - lpRect->top < st_szInitDlgSize.cy) {
			lpRect->top = lpRect->bottom - st_szInitDlgSize.cy;
			bSizeFixed = TRUE;
		}
		break;
	case WMSZ_BOTTOM:
	case WMSZ_BOTTOMLEFT:
	case WMSZ_BOTTOMRIGHT:
		if (lpRect->bottom - lpRect->top < st_szInitDlgSize.cy) {
			lpRect->bottom = lpRect->top + st_szInitDlgSize.cy;
			bSizeFixed = TRUE;
		}
		break;
	}

	RECT rc;
	GetWindowRect(hwnd, &rc);
	int nDeltaInc = (lpRect->bottom - lpRect->top) - (rc.bottom - rc.top);
	ReposControls(hwnd, nDeltaInc, 0, 0);
	g_nHeightDelta += nDeltaInc;
/*
	TCHAR b[1024];
	wsprintf(b, _T("%d, %d, %d\n"), g_nHeightDelta, g_nLocalesDelta, g_nGroupsDelta);
	OutputDebugString(b);
*/
	return bSizeFixed;
}


UINT OnNcHitTest(HWND hwnd, int x, int y)
{
	LRESULT lResult = FORWARD_WM_NCHITTEST(hwnd, x, y, DefWindowProc);

	while (HTCLIENT == lResult) {
		POINT pt = { x, y };
		MapWindowPoints(HWND_DESKTOP, hwnd, &pt, 1);

		if (pt.x < st_aControls[eHotkeysList].rc.left
				|| pt.x > st_aControls[eHotkeysList].rc.right)
			break;

		if (pt.y < st_aControls[eHotkeysList].rc.bottom
				|| pt.y > st_aControls[eGroupsList].rc.top)
			break;

		if (pt.y < st_aControls[eLocalesList].rc.top)
			lResult = HTSPLIT_TOP;
		if (pt.y > st_aControls[eLocalesList].rc.bottom)
			lResult = HTSPLIT_BOTTOM;

		break;
	}

	SetDlgMsgResult(hwnd, WM_NCHITTEST, lResult);
	return TRUE;
}


BOOL OnSetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg)
{
	BOOL bResult = TRUE;
	switch (codeHitTest) {
	case HTSPLIT_TOP:
	case HTSPLIT_BOTTOM:
		SetCursor(LoadCursor(NULL, IDC_SIZENS));
		break;
	default:
		bResult = FORWARD_WM_SETCURSOR(hwnd, hwndCursor, codeHitTest, msg, DefWindowProc);
		break;
	}
		
	SetDlgMsgResult(hwnd, WM_NCHITTEST, bResult);
	return TRUE;
}


void OnNCLButtonDown(HWND hwnd, BOOL /*fDoubleClick*/, int x, int y, UINT codeHitTest)
{
	switch (codeHitTest) {
	case HTSPLIT_TOP:
	case HTSPLIT_BOTTOM: {
		SetCapture(hwnd);
		st_uiCapturedHitTest = codeHitTest;
		POINT pt = { x, y };
		MapWindowPoints(HWND_DESKTOP, hwnd, &pt, 1);
		st_nCapturedY = pt.y;
		break;
	}
	default:
		break;
	}
}


void OnMouseMove(HWND hwnd, int /*x*/, int y, UINT /*keyFlags*/)
{
	if (hwnd == GetCapture()) {
		int nLocalesDelta = 0;
		int nGroupsDelta = 0;
		switch (st_uiCapturedHitTest) {
		case HTSPLIT_TOP: {
			int nMin = st_nInitHotkeysHeight;
			nMin += st_aControls[eHotkeysList].rc.top;
			if (y < nMin)
				y = nMin;
			int nMax = -st_nInitLocalesHeight;
			nMax += st_aControls[eLocalesList].rc.bottom;
			if (y > nMax)
				y = nMax;
			ReposControls(hwnd, 0, nLocalesDelta = st_nCapturedY - y, 0);
			st_nCapturedY = y;
			break;
		}
		case HTSPLIT_BOTTOM: {
			int nMin = st_aControls[eLocalesList].rc.top + st_nInitLocalesHeight;
			int nMax = st_aControls[eGroupsList].rc.bottom - st_nInitGroupsHeight;
			if (y < nMin)
				y = nMin;
			if (y > nMax)
				y = nMax;
			ReposControls(hwnd, 0, nLocalesDelta = y - st_nCapturedY,
								nGroupsDelta = st_nCapturedY - y);
			st_nCapturedY = y;
			break;
		}
		}

		g_nLocalesDelta += nLocalesDelta;
		g_nGroupsDelta += nGroupsDelta;
	}
}


void OnLButtonUp(HWND hwnd, int /*x*/, int /*y*/, UINT /*keyFlags*/)
{
  if (hwnd == GetCapture()) {
		ReleaseCapture();
		st_uiCapturedHitTest = 0;
	}
}


static INT_PTR __stdcall ConfigureDialogProc(HWND hDlg, UINT message,
									WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		HANDLE_MSG(hDlg, WM_INITDIALOG, OnInitConfigureDialog);
		HANDLE_MSG(hDlg, WM_COMMAND, OnConfigureCommand);
		HANDLE_MSG(hDlg, WM_NOTIFY, OnConfigureNotify);
		HANDLE_MSG(hDlg, WM_DRAWITEM, OnConfigureDrawItem);
		HANDLE_MSG(hDlg, WM_SIZING, OnSizing);
		HANDLE_MSG(hDlg, WM_NCHITTEST, OnNcHitTest);
		HANDLE_MSG(hDlg, WM_SETCURSOR, OnSetCursor);
		HANDLE_MSG(hDlg, WM_NCLBUTTONDOWN, OnNCLButtonDown);
		HANDLE_MSG(hDlg, WM_MOUSEMOVE, OnMouseMove);
		HANDLE_MSG(hDlg, WM_LBUTTONUP, OnLButtonUp);
		default: break;
	}
	return FALSE;
}


int RunConfiguration(HWND hParentWnd)
{
	return ( int )DialogBox(g_hInstance,
		MAKEINTRESOURCE(IDD_CONFIGURE_DIALOG),    /// dialog box template
		hParentWnd, ConfigureDialogProc);
}


bool LoadExternalIcon(HICON* phIcon, LPCTSTR lpszName)
{
	OPENFILENAME ofn = { 0 };
	TCHAR szPath[MAX_PATH] = { 0 };
	TCHAR szDrive[_MAX_DRIVE] = { 0 };
	TCHAR szDir[_MAX_DIR] = { 0 };
	GetModuleFileName(g_hInstance, szPath, _countof(szPath));
	_tsplitpath(szPath, szDrive, szDir, NULL, NULL);
	_tmakepath(szPath, szDrive, szDir, _T("flags"), NULL);
	szDir[0] = '\0';

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_hwndMessageWindow;
	ofn.lpstrFile = szDir;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = _T("Icons\0*.ico\0");
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = szPath;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	std::basic_string<TCHAR> strTitle(_T("Select icon for layout "));
	strTitle += lpszName;
	ofn.lpstrTitle = strTitle.c_str();

	if (TRUE == GetOpenFileName(&ofn)) {
		HICON hIcon = (HICON)LoadImage(NULL,
			ofn.lpstrFile, IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
		DestroyIcon(*phIcon);
		*phIcon = hIcon;
		return true;
	}

	return false;
}


////////////////////////////////////////////////////////////////////////////
//
//  About Box dialog
//

static HWND st_hwndAboutBox = NULL;

static BOOL OnInitAboutBoxDialog(HWND hwnd, HWND /*hwndFocus*/, LPARAM /*lParam*/)
{
	st_hwndAboutBox = hwnd;

	SendMessage(hwnd, WM_SETICON, ICON_SMALL,
		(LPARAM)LoadIcon(NULL, IDI_INFORMATION));

	std::vector<TCHAR> path;
	path.resize(MAX_PATH * 3);
	DWORD handle = 0;
	DWORD dataSize = 0;
	if (GetModuleFileName(g_hInstance, &path[0], ( DWORD )path.size())
			&& (0 != (dataSize = GetFileVersionInfoSize(&path[0], &handle)))) {
		void* verData = alloca(dataSize);
		TCHAR* pverInfo = NULL;
		UINT uiSize = 0;
		if (GetFileVersionInfo(&path[0], handle, dataSize, verData)
				&& VerQueryValue(verData,
					_T("\\StringFileInfo\\040904b0\\ProductVersion"),
					(void**)&pverInfo, &uiSize)) {
			SetWindowText(GetDlgItem(hwnd, IDC_VERSION_STATIC), pverInfo);
		}
	}

	return TRUE;
}


static void OnAboutBoxCommand(
		HWND hwnd, int id, HWND /*hwndCtl*/, UINT /*codeNotify*/)
{
	switch (id) {
	case IDOK:			EndDialog(hwnd, IDOK); break;
	case IDCANCEL:	EndDialog(hwnd, IDCANCEL); break;
	default:
		break;
	}
}


static LRESULT OnAboutBoxNotify(HWND hwnd, int /*idFrom*/, NMHDR* pnmhdr)
{
	if (NULL == pnmhdr)
		return 0;

	switch (pnmhdr->code) {
	case NM_CLICK:
		switch (pnmhdr->idFrom) {
		case IDC_SF_LINK:
			ShellExecute(NULL, _T("open"),
				_T("http://recaps.sourceforge.io"),
				NULL, NULL, SW_SHOWNORMAL);
			break;
		case IDC_PAYPAL_LINK: return OnDonationClick(hwnd, pnmhdr);
		}
		break;
	default:
		break;
	}

	return 0;
}


static INT_PTR __stdcall AboutBoxCutDialogProc(HWND hDlg, UINT message,
															WPARAM wParam, LPARAM lParam)
{
	switch (message) {
		HANDLE_MSG(hDlg, WM_INITDIALOG, OnInitAboutBoxDialog);
		HANDLE_MSG(hDlg, WM_COMMAND, OnAboutBoxCommand);
		HANDLE_MSG(hDlg, WM_NOTIFY, OnAboutBoxNotify);
		default: break;
	}
	return FALSE;
}


int AboutBoxDialog(HWND hParentWnd)
{
	MessageBeep(MB_ICONINFORMATION);

	if (NULL == st_hwndAboutBox) {
		int nResult = ( int )DialogBox(g_hInstance,
			MAKEINTRESOURCE(IDD_ABOUT_BOX),    /// dialog box template
			hParentWnd, AboutBoxCutDialogProc);

		st_hwndAboutBox = NULL;
		return nResult;
	}

	SetFocus(st_hwndAboutBox);
	return IDCANCEL;
}

