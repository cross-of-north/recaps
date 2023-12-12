#pragma once

void LoadConfiguration(std::map<HKL, KeyboardLayoutInfo>& info);
void SaveConfiguration(const std::map<HKL, KeyboardLayoutInfo>& info);

HICON CreateGrayIcon(HICON hIconColor);

bool RegCheckAndSetRunAtLogon(bool bToggleState);

