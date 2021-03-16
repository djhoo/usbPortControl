#pragma once
#ifndef _usb_block_header_h_
#define _usb_block_header_h_

int GetUSBDriveLetters(TCHAR* pDriveLetters, int size);
bool GetDriveDeviceIDByDriveLetter(TCHAR driveLetter, wstring& csDeviceID);
DWORD WINAPI ThreadFunc_Window(LPVOID lpParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
ATOM MyRegisterClass();
bool CreateMessageOnlyWindow();
void RegisterDeviceNotify();
void InitWhiteIDList(const TCHAR* _filename);
bool AddWhiteID(wstring& csDeviceID);
bool InWhiteIDList(wstring& csDeviceID);
bool IsUDrive(TCHAR driveLetter); //判断是否是U盘
bool IsRemovableCDRom(TCHAR driveLetter);//判断是否是USB光盘
bool IsRemovableDrive(TCHAR driveLetter);//判断是可移动硬盘 true:移动硬盘，false:本地硬盘 
INT GetBusType(TCHAR driveLetter);
int DriveLettersFromMask(ULONG unitmask, TCHAR* pDriveLetters, int size);
bool GetDeviceIDByDevInst(DEVINST DevInst, wstring& csDeviceID);
void UnRegisterDeviceNotify();
LRESULT DeviceChange(UINT message, WPARAM wParam, LPARAM lParam);
void UpdateDevice(PDEV_BROADCAST_DEVICEINTERFACE pDevInf, WPARAM wParam);
void UpdateVolume(PDEV_BROADCAST_VOLUME pDevVolume, WPARAM wParam);
DWORD WINAPI EjectVolume(LPVOID lpParam);
void CheckVolume(PDEV_BROADCAST_VOLUME pDevVolume, WPARAM wParam);
BOOL EjectDriveParrentDeviceByDriveLetter(TCHAR driveLetter);
DEVINST GetDrivesDevInstByDeviceNumber_DeviceType(DWORD DeviceNumber, DEVICE_TYPE DeviceType, TCHAR driveLetter);
int StartApp(const TCHAR* configFile);
void ShowHelp();
void ListSystemUSBDiskDeviceID(const TCHAR* pOutFile);
void EjectSystemInvalidUSBDisk();
bool IsValidDriveLetter(TCHAR driveLetter);
#endif /* _usb_block_header_h_ */