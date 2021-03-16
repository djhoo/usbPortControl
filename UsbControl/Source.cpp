// Source.cpp
#include <windows.h>
#include <tchar.h>
#include <string>
#include <fstream>
#include <xstring>
#include <list>
#include <algorithm> 
#include <cctype>
#include <functional> 

#include <Dbt.h>
#include <setupapi.h>  //SetupDi系列函数
#include <cfgmgr32.h>
#pragma comment(lib, "setupapi.lib")

using namespace std;
#include "Header.h"

#define APP_EXIT_MESSAGE WM_USER + 1
const TCHAR CMD_HELP[] = TEXT("/?");
const TCHAR CMD_LISTID[] = TEXT("/L");
const TCHAR CLASS_NAME[] = TEXT("USB Blocker Window Class");
const TCHAR CommentChar = TEXT(';');
const TCHAR ValidUSBIDFlag[] = TEXT("USB\\VID_");

HWND g_hWnd = NULL;
DWORD g_WindowThreadId = 0;
HANDLE g_WindowThreadHandle = NULL;
HDEVNOTIFY g_NotifyDevHandle = NULL;
list<wstring> g_WhiteIDList;
wstring g_AppPath;

inline wstring& ltrim(wstring &str) {
	wstring::iterator p = find_if(str.begin(), str.end(), not1(ptr_fun<int, int>(isspace)));
	str.erase(str.begin(), p);
	return str;
}
inline wstring& rtrim(wstring &str) {
	wstring::reverse_iterator p = find_if(str.rbegin(), str.rend(), not1(ptr_fun<int, int>(isspace)));
	str.erase(p.base(), str.end());
	return str;
}
inline wstring& trim(wstring &str) {
	ltrim(rtrim(str));
	return str;
}

BOOL WINAPI ConsoleHandler(DWORD CEvent)
{
	//return true;//表示阻止响应系统对该程序的操作
	//return false;//忽略处理，让系统进行默认操作
	BOOL ret = false;
	switch (CEvent)
	{
	case CTRL_C_EVENT: // Ctrl+C 事件
		//_tprintf(TEXT("-- CTRL_C_EVENT --\n"));
		ret = true;
		break;
	case CTRL_BREAK_EVENT:
		//_tprintf(TEXT("-- CTRL_BREAK_EVENT --\n"));
		PostThreadMessage(g_WindowThreadId, APP_EXIT_MESSAGE, 0, 0);
		ret = true;
		break;
		//case CTRL_CLOSE_EVENT: //用户点 X 关闭事件
		//	_tprintf(TEXT("-- CTRL_CLOSE_EVENT --\n"));
		//	break;
		//case CTRL_LOGOFF_EVENT:
		//	_tprintf(TEXT("LOGOFF...\n"));
		//	break;
		//case CTRL_SHUTDOWN_EVENT: //系统关闭事件
		//	_tprintf(TEXT("SHUTDOWN...\n"));
		//	break;
			//CTRL_CLOSE_EVENT
	}
	return ret;
}

static const GUID GUID_DEVINTERFACE_LIST[] =
{
	// GUID_DEVINTERFACE_USB_DEVICE
	{ 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } },

	// GUID_DEVINTERFACE_DISK
	//{ 0x53f56307, 0xb6bf, 0x11d0, { 0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b } },

	// GUID_DEVINTERFACE_HID, 
	//{ 0x4D1E55B2, 0xF16F, 0x11CF, { 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } },

	// GUID_NDIS_LAN_CLASS
	//{ 0xad498944, 0x762f, 0x11d0, { 0x8d, 0xcb, 0x00, 0xc0, 0x4f, 0xc3, 0x35, 0x8c } }

	//GUID_DEVINTERFACE_COMPORT
	//{ 0x86e0d1e0, 0x8089, 0x11d0, { 0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73 } },

	//GUID_DEVINTERFACE_SERENUM_BUS_ENUMERATOR
	//{ 0x4D36E978, 0xE325, 0x11CE, { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } },

	//GUID_DEVINTERFACE_PARALLEL
	//{ 0x97F76EF0, 0xF883, 0x11D0, { 0xAF, 0x1F, 0x00, 0x00, 0xF8, 0x00, 0x84, 0x5C } },

	//GUID_DEVINTERFACE_PARCLASS
	//{ 0x811FC6A5, 0xF728, 0x11D0, { 0xA5, 0x37, 0x00, 0x00, 0xF8, 0x75, 0x3E, 0xD1 } }
};

void InitWhiteIDList(const TCHAR* pInFile)
{
	g_WhiteIDList.clear();
	wifstream hFile;

	if (pInFile) {
		hFile.open(pInFile, wios::in);
	}

	if (hFile.is_open()) {
		_tprintf(TEXT("valid usb storage device ID:\n"));
		_tprintf(TEXT("--------------------------------------------------\n"));
		wstring strLine;
		while (getline(hFile, strLine))
		{
			bool bret = AddWhiteID(strLine);
			if (bret) {
				_tprintf(TEXT("%s\n"), strLine.c_str());
			}
		}
		_tprintf(TEXT("--------------------------------------------------\n"));
		hFile.close();
	}
	else
	{
		_tprintf(TEXT("block all usb storage device.\n"));
	}
}

int GetUSBDriveLetters(TCHAR* pDriveLetters, int size)
{
	int ret = 0;
	if (!pDriveLetters || size < 26) {
		return 0;
	}
	else
	{
		for (int is = 0; is < size; is++) {
			*(pDriveLetters + is) = 0;
		}
	}

	DWORD dwDrives = GetLogicalDrives();
	TCHAR Letters[26] = { 0 };
	int cnt = DriveLettersFromMask((ULONG)dwDrives, Letters, sizeof(Letters) / sizeof(Letters[0]));
	ret = 0;
	for (int i = 0; i < cnt; i++)
	{
		TCHAR driveLetter = Letters[i];
		if (IsUDrive(driveLetter) 
			|| IsRemovableDrive(driveLetter)
			|| IsRemovableCDRom(driveLetter)
			)
		{
			*(pDriveLetters + ret) = driveLetter;
			ret++;
		}
	}

	//_tprintf(TEXT("USBDrive Letters: %s\n"), pDriveLetters);
	return ret;
}

bool GetDriveDeviceIDByDriveLetter(TCHAR driveLetter, wstring& csDeviceID)
{
	if (!IsValidDriveLetter(driveLetter))
	{
		return false;
	}

	TCHAR tszSymbol[MAX_PATH] = { TEXT("\\\\?\\*:") };
	tszSymbol[4] = driveLetter;

	BOOL bRet = FALSE;
	DWORD sdn_DeviceNumber = 0;
	DEVICE_TYPE sdn_DeviceType;
	DWORD lastErr = 0;

	HANDLE   hDevice = CreateFile(tszSymbol, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		lastErr = GetLastError();
		return false;
	}

	//通过DeviceIoControl获取设备的STORAGE_DEVICE_NUMBER
	STORAGE_DEVICE_NUMBER  sdn;  //包含有关设备的信息。
	DWORD  dwBytesReturned = 0;
	bRet = DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &dwBytesReturned, NULL);
	CloseHandle(hDevice);
	if (!bRet)
	{
		return false;
	}
	sdn_DeviceNumber = sdn.DeviceNumber;
	sdn_DeviceType = sdn.DeviceType;
	GUID Guid;

	if (FILE_DEVICE_DISK == sdn_DeviceType)
	{
		Guid = GUID_DEVINTERFACE_DISK;
	}
	else if ( FILE_DEVICE_CD_ROM == sdn_DeviceType)
	{
		Guid = GUID_DEVINTERFACE_CDROM;
	}
	else
	{
		return false;
	}

	//
	//已根据盘符得到设备的STORAGE_DEVICE_NUMBER,然后枚举USB设备,
	//找到相同的STORAGE_DEVICE_NUMBER的设备, (只需比较DeviceNumber和DeviceType)
	HDEVINFO hDevInfo = SetupDiGetClassDevs(&Guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (hDevInfo == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	SP_DEVINFO_DATA                  DevInfoData;
	DevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	SP_DEVICE_INTERFACE_DATA         DevInterfaceData;
	DevInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	//SetupDiGetDeviceInterfaceDetail用于存放信息,需要根据实际大小动态分配
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetailData = NULL;

	DWORD dwRequiredSize = 0;    //保存SetupDiGetDeviceInterfaceDetail返回的实际需要的缓存区大小
	DEVINST DevInst = 0;        //设备Instance

	//枚举所有设备
	DWORD dwIndex = 0;
	BOOL bFound = false;
	CONFIGRET configRet = CR_SUCCESS;
	while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &Guid, dwIndex, &DevInterfaceData))
	{
		//先直接调用函数,目的是获取第一个设备所需要的缓存区大小dwRequiredSize;
		SetupDiGetDeviceInterfaceDetail(hDevInfo, &DevInterfaceData, NULL, 0, &dwRequiredSize, &DevInfoData);

		//得到大小后分配内存
		pDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(dwRequiredSize);
		pDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		//再次调用获取信息
		if (!SetupDiGetDeviceInterfaceDetail(hDevInfo, &DevInterfaceData, pDeviceInterfaceDetailData, dwRequiredSize, &dwRequiredSize, &DevInfoData))
		{
			free(pDeviceInterfaceDetailData);
			SetupDiDestroyDeviceInfoList(hDevInfo);
			return false;
		}

		HANDLE hDrive = CreateFile(pDeviceInterfaceDetailData->DevicePath, 0, 
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hDrive == INVALID_HANDLE_VALUE)
		{
			free(pDeviceInterfaceDetailData);
			SetupDiDestroyDeviceInfoList(hDevInfo);
			return false;
		}

		STORAGE_DEVICE_NUMBER sdnEnum; //每次枚举时获取的设备信息
		bRet = DeviceIoControl(hDrive,
			IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL, 0, &sdnEnum, sizeof(sdnEnum),
			&dwBytesReturned, NULL);

		if (bRet)
		{
			// 比较由盘符获取的DeviceNumber,DeviceType与枚举时获取的DeviceNumber,DeviceType,相同则说明盘符对应此USB设备
			if (sdn_DeviceNumber == sdnEnum.DeviceNumber && sdn_DeviceType == sdnEnum.DeviceType)
			{
				DevInst = DevInfoData.DevInst;
				bFound = TRUE;
			}
		}

		free(pDeviceInterfaceDetailData);
		CloseHandle(hDrive);
		dwIndex++;
	}

	if (!bFound)
	{
		SetupDiDestroyDeviceInfoList(hDevInfo);
		return false;
	}

	//获取Parent DevInst
	DEVINST DevInstParent = 0;
	configRet = CM_Get_Parent(&DevInstParent, DevInst, 0);
	if (configRet == CR_SUCCESS)
	{
		if (GetDeviceIDByDevInst(DevInstParent, csDeviceID))
		{
			SetupDiDestroyDeviceInfoList(hDevInfo);
			return true;
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return false;
}

DWORD WINAPI ThreadFunc_Window(LPVOID lpParam)
{
	if (0 == MyRegisterClass())
		return -1;

	if (!CreateMessageOnlyWindow())
		return -1;

	RegisterDeviceNotify();

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (msg.message == APP_EXIT_MESSAGE)
		{
			_tprintf(TEXT("worker receive the exiting Message...\n"));
			return 0;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_NCCREATE://WM_NCCREATE 消息是第一条消息, 这是一个创建你自己的实例变量的好地方. 需要注意的是, 如果你导致 WM_NCCREATE 消息返回失败, 那么所有你将收到的消息就只有 WM_NCDESTROY 了; 不会有 WM_DESTROY 消息了, 因为你根本就没有收到相应的 WM_CREATE 消息
		//_tprintf(L"WM_NCCREATE\n");
		break;
	case  WM_CREATE:
		/*WM_CREATE - 在窗口创建成功还未显示之前, 收到这个消息. 	WPARAM - 不使用 	LPARAM - 是CREATESTRUCT结构类型的指针 该消息是非队列消息.
		WM_CREATE是所有窗口都能响应的消息，表明本窗口已经创建完毕（可以安全的使用这个窗口了，例如在它上面画控件等）。
		在响应WM_CREATE消息响应函数的时候，对话框及子控件还未创建完成，亦是说只是通知系统说要开始创建窗口啦，
		这个消息响应完之后，对话框和子控件才开始创建。因此在此消息响应函数中无法对控件进行修改和初始化
		*/
		//_tprintf(L"WM_CREATE\n");
		break;

	case WM_DEVICECHANGE:
		//WM_DEVICECHANGE 里默认是只注册了 DBT_DEVICEREMOVECOMPLETE 和 DBT_DEVICEARRIVAL 消息，所以只能接收到这两个，
		//如果要使用其他的消息，必需手动注册一下才可以
		//_tprintf(L"WM_DEVICECHANGE\n");
		return DeviceChange(message, wParam, lParam);
		break;
	case WM_CLOSE:
		//_tprintf(L"WM_CLOSE\n");
		break;
	case WM_DESTROY:
		//WM_DESTROY 消息是在窗口销毁动作序列中的开始被发送的, 而 WM_NCDESTROY 消息是在结尾
		//_tprintf(L"WM_DESTROY\n");
		PostQuitMessage(0);
		break;
	case WM_NCDESTROY:
		//WM_NCDESTROY 消息是你窗口将会收到的最后一个消息 (在没有怪诞行为影响的前提下), 因此, 这里是做 "最终清理" 的最佳场所
		//_tprintf(L"WM_NCDESTROY\n");
		UnRegisterDeviceNotify();
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

bool InWhiteIDList(wstring& csDeviceID)
{
	wstring id(csDeviceID);
	if (g_WhiteIDList.empty())
	{
		return false;
	}

	transform(id.begin(), id.end(), id.begin(), ::toupper); //to upper
	//find
	list<wstring>::iterator iter = find(g_WhiteIDList.begin(), g_WhiteIDList.end(), id);
	//iter == g_WhiteIDList.end() means not found
	return (iter == g_WhiteIDList.end() ? false : true);
}

bool AddWhiteID(wstring& csDeviceID)
{
	// USB\VID_125F&PID_312B\1108170000000028
	bool bRet = false;

	do
	{
		wstring id(csDeviceID);
		trim(id);//trim left & right space
		if (id.length() == 0)
		{
			break;
		}
		wstring::size_type position;
		position = id.find(CommentChar); //  position == id.npos means not found
		if (0 == position)
		{// start with ;  is comment line
			break;
		}
		transform(id.begin(), id.end(), id.begin(), ::toupper); //to upper

		position = id.find(ValidUSBIDFlag);
		if (0 == position && !InWhiteIDList(id))
		{//USB\VID_125F&PID_312B\1108170000000028  , start with USB\VID_
			g_WhiteIDList.push_back(id);
			bRet = true;
		}
	} while (false);

	return bRet;
}

INT GetBusType(TCHAR driveLetter)
{ //return _STORAGE_BUS_TYPE

	HANDLE hDevice = INVALID_HANDLE_VALUE; // handle to the drive to be examined
	STORAGE_DESCRIPTOR_HEADER *pDevDescHeader = NULL;
	STORAGE_DEVICE_DESCRIPTOR *pDevDesc = NULL;
	INT BusType = BusTypeUnknown;

	do
	{
		if (!IsValidDriveLetter(driveLetter))
		{
			break;
		}
		TCHAR tszSymbol[MAX_PATH] = { TEXT("\\\\?\\*:") };
		tszSymbol[4] = driveLetter;

		BOOL bResult = FALSE; // results flag
		DWORD readed = 0; // discard results
		DWORD lastErr = 0;
		DWORD devDescLength = 0;
		STORAGE_PROPERTY_QUERY query;

		memset(&query, 0, sizeof(query));

		hDevice = CreateFile(tszSymbol, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

		if (hDevice == INVALID_HANDLE_VALUE) // cannot open the drive
		{
			lastErr = GetLastError(); // 5 - 拒绝访问

			break;
		}

		pDevDescHeader = (STORAGE_DESCRIPTOR_HEADER *)malloc(sizeof(STORAGE_DESCRIPTOR_HEADER));

		query.PropertyId = StorageDeviceProperty;
		query.QueryType = PropertyStandardQuery;

		bResult = DeviceIoControl(hDevice,     // device to be queried
			IOCTL_STORAGE_QUERY_PROPERTY,     // operation to perform
			&query, sizeof(query),               // no input buffer
			pDevDescHeader, sizeof(STORAGE_DESCRIPTOR_HEADER),     // output buffer
			&readed,                 // # bytes returned
			NULL);      // synchronous I/O
		if (!bResult)        //fail
		{
			break;
		}

		devDescLength = pDevDescHeader->Size;
		pDevDesc = (STORAGE_DEVICE_DESCRIPTOR *)malloc(devDescLength);
		if (NULL == pDevDesc)
		{
			break;
		}

		bResult = DeviceIoControl(hDevice,     // device to be queried
			IOCTL_STORAGE_QUERY_PROPERTY,     // operation to perform
			&query, sizeof query,               // no input buffer
			pDevDesc, devDescLength,     // output buffer
			&readed,                 // # bytes returned
			NULL);      // synchronous I/O
		if (!bResult)        //fail
		{
			break;
		}

		BusType = pDevDesc->BusType;
	} while (false);

	if (NULL != pDevDescHeader)
	{
		free(pDevDescHeader);
		pDevDescHeader = NULL;
	}


	if (NULL != pDevDesc)
	{
		free(pDevDesc);
		pDevDesc = NULL;
	}

	if (hDevice != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hDevice);
		hDevice = INVALID_HANDLE_VALUE;
	}
	return BusType;
}
//判断是否是可移动光驱
bool IsRemovableCDRom(TCHAR driveLetter)
{
	bool bRemovableCDRom = false;
	UINT DriverType = DRIVE_UNKNOWN;
	TCHAR tszDriver[4] = { TEXT("*:\\") };

	tszDriver[0] = driveLetter;
	DriverType = GetDriveType(tszDriver);

	if (DriverType == DRIVE_CDROM) 
	{
		wstring csDeviceID;
		if (GetDriveDeviceIDByDriveLetter(driveLetter, csDeviceID)) 
		{
			transform(csDeviceID.begin(), csDeviceID.end(), csDeviceID.begin(), ::toupper); //to upper
			if (csDeviceID.find(ValidUSBIDFlag) == 0) {
				bRemovableCDRom = true;
			}
		}
	}
	
	//return DriverType == DRIVE_CDROM;
	return bRemovableCDRom;
}

//判断是可移动硬盘 true:移动硬盘，false:本地硬盘 
bool IsRemovableDrive(TCHAR driveLetter)
{
	bool bRemovableDrive = false;
	UINT DriverType = DRIVE_UNKNOWN;
	TCHAR tszDriver[4] = { TEXT("*:\\") };


	tszDriver[0] = driveLetter;
	DriverType = GetDriveType(tszDriver);

	if (DriverType == DRIVE_FIXED)
	{
		if (GetBusType(driveLetter) == BusTypeUsb)
		{
			bRemovableDrive = true;
		}
	}

	return bRemovableDrive;
}

//判断是否是U盘
bool IsUDrive(TCHAR driveLetter)
{
	TCHAR tszDriver[4] = { TEXT("*:\\") };
	UINT DriverType = DRIVE_UNKNOWN;
	tszDriver[0] = driveLetter;
	DriverType = GetDriveType(tszDriver);

	return DriverType == DRIVE_REMOVABLE;
}

int DriveLettersFromMask(ULONG unitmask, TCHAR* pDriveLetters, int size)
{
	int ret = 0;
	if (!pDriveLetters || size < 26) {
		return 0;
	}
	else
	{
		for (int is = 0; is < size; is++) {
			*(pDriveLetters + is) = 0;
		}
	}

	TCHAR i;
	for (i = 0; i < 26; ++i)
	{
		if (unitmask & 0x1) {
			*pDriveLetters = i + TEXT('A');
			pDriveLetters++;
			ret++;
		}
		unitmask = unitmask >> 1;
	}
	return ret;
}

bool GetDeviceIDByDevInst(DEVINST DevInst, wstring& csDeviceID)
{
	ULONG  IDLen = 0;
	CONFIGRET configRet = CM_Get_Device_ID_Size(&IDLen, DevInst, 0);
	if (configRet == CR_SUCCESS)
	{
		IDLen++; //plus 1 to allow room for the string's terminating NULL 
		TCHAR* pIDBuffer = (TCHAR*)malloc(IDLen * sizeof(TCHAR));
		if (pIDBuffer)
		{
			configRet = CM_Get_Device_ID(DevInst, pIDBuffer, IDLen, 0); // 得到设备ID,如 USB\VID_17EF&PID_3801\907117000F7F
			if (configRet == CR_SUCCESS)
			{
				csDeviceID.assign(pIDBuffer);    //找到的设备的ID
				transform(csDeviceID.begin(), csDeviceID.end(), csDeviceID.begin(), ::toupper); //to upper
				free(pIDBuffer);
				return true;
			}
			free(pIDBuffer);
		}
	}
	return false;
}

void UnRegisterDeviceNotify()
{
	if (NULL != g_NotifyDevHandle) {
		UnregisterDeviceNotification(g_NotifyDevHandle);
	}
}

LRESULT DeviceChange(UINT message, WPARAM wParam, LPARAM lParam)
{
	/*
		当USB设备插入或者弹出时，Windows会产生一条全局消息：WM_DEVICECHANGE
	我们需要做的是，获得这条消息的wParam参数，如果为DBT_DEVICEARRIVAL则表示有设备插入并可用，
	如果是DBT_DEVICEREMOVECOMPLETE则表示有设备已经移除。再查看lParam参数为DBT_DEVTYP_VOLUME时，
	就可以取出DEV_BROADCAST_VOLUME结构的卷号dbcv_unitmask，就知道是哪个卷被插入或者弹出。
	*/
	if (DBT_DEVICEARRIVAL == wParam  // 表示有设备插入并可用
		|| DBT_DEVICEREMOVECOMPLETE == wParam // 设备移除
		)
	{
		PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
		PDEV_BROADCAST_DEVICEINTERFACE pDevInf;
		//PDEV_BROADCAST_HANDLE pDevHnd;
		//PDEV_BROADCAST_OEM pDevOem;
		//PDEV_BROADCAST_PORT pDevPort;
		PDEV_BROADCAST_VOLUME pDevVolume;

		switch (pHdr->dbch_devicetype)
		{
		case DBT_DEVTYP_DEVICEINTERFACE:
			pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr;
			UpdateDevice(pDevInf, wParam);
			break;

			//case DBT_DEVTYP_HANDLE:
			//	pDevHnd = (PDEV_BROADCAST_HANDLE)pHdr;
			//	_tprintf(L"DBT_DEVTYP_HANDLE\n");
			//	break;

			//case DBT_DEVTYP_OEM:
			//	pDevOem = (PDEV_BROADCAST_OEM)pHdr;
			//	_tprintf(L"DBT_DEVTYP_OEM\n");
			//	break;

			//case DBT_DEVTYP_PORT:
			//	pDevPort = (PDEV_BROADCAST_PORT)pHdr;
			//	_tprintf(L"DBT_DEVTYP_PORT\n");
			//	break;

		case DBT_DEVTYP_VOLUME:
			pDevVolume = (PDEV_BROADCAST_VOLUME)pHdr;
			UpdateVolume(pDevVolume, wParam);
			break;
		}
	}
	return 0;
}

void UpdateVolume(PDEV_BROADCAST_VOLUME pDevVolume, WPARAM wParam)
{
	CheckVolume(pDevVolume, wParam);
}

void UpdateDevice(PDEV_BROADCAST_DEVICEINTERFACE pDevInf, WPARAM wParam)
{
	if (DBT_DEVICEARRIVAL != wParam && DBT_DEVICEREMOVECOMPLETE != wParam)
	{
		return;
	}

	//pDevInf->dbcc_name: \\?\USB#VID_125F&PID_312B#1108170000000028#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
	wstring szDevId(pDevInf->dbcc_name + 4); // USB#VID_125F&PID_312B#1108170000000028#{a5dcbf10-6530-11d2-901f-00c04fb951ed}
	size_t idx = szDevId.find_last_of(TEXT('#'));
	szDevId = szDevId.substr(0, idx); //USB#VID_125F&PID_312B#1108170000000028
	replace(szDevId.begin(), szDevId.end(), TEXT('#'), TEXT('\\')); //USB\VID_125F&PID_312B\1108170000000028
	//to upper
	transform(szDevId.begin(), szDevId.end(), szDevId.begin(), ::toupper);  //USB\VID_125F&PID_312B\1108170000000028

	DWORD devicetype = pDevInf->dbcc_devicetype;

	if (DBT_DEVICEARRIVAL == wParam) {
		_tprintf(TEXT("add device: %s\n"), szDevId.c_str());
	}
	else {
		_tprintf(TEXT("remove device: %s\n"), szDevId.c_str());
	}

}

void CheckVolume(PDEV_BROADCAST_VOLUME pDevVolume, WPARAM wParam)
{
	/*
用DeviceIoControl函数,先通过设备消息得到新加入的du盘符并且排除映射盘(net use/subst).
net use s: \\172.25.78.63\usb rst200233 /user:john
net use s: /delete

subst x: g:\aa01
subst x: /d
然后通过DeviceIoControl函数发送IOCTL_STORAGE_BASE equ FILE_DEVICE_MASS_STORAGE得到盘符的总线类别,
而不是通过一般的GetDirverTyte,或是网上有些人说的IOCTL_STORAGE_GET_MEDIA_TYPES.最后通过GetDirverTyte排除USB光驱
	*/

	if (NULL == pDevVolume
		|| 0 != pDevVolume->dbcv_flags //dbcv_flags 表示驱动器的类别，0 : 硬盘、U盘; 1 : 光盘驱动器；2: 网络驱动器
		)
	{
		return;
	}

	if (DBT_DEVICEARRIVAL == wParam || DBT_DEVICEREMOVECOMPLETE == wParam)
	{
		DWORD unitmask = pDevVolume->dbcv_unitmask;
		TCHAR driveLetters[26] = { 0 };
		int cnt = DriveLettersFromMask(unitmask, driveLetters, 26);
		if (DBT_DEVICEARRIVAL == wParam) {
			for (int i = 0; i < cnt; i++)
			{
				_tprintf(TEXT("add drive: %c:\n"), driveLetters[i]);
#if 1 //ejectvolume in thread
				DWORD ThreadId = 0;
				HANDLE ThreadHandle = CreateThread(NULL, 0, EjectVolume, (LPVOID)driveLetters[i], 0, &ThreadId);
				if (ThreadHandle == NULL) {
					_tprintf(TEXT("create thread EjectVolume error:%c\n"), driveLetters[i]);
				}
#else
				DWORD dwRet = EjectVolume((LPVOID)driveLetters[i]);
#endif
			}

		}
		else
		{
			for (int i = 0; i < cnt; i++)
			{
				_tprintf(TEXT("remove drive: %c:\n"), driveLetters[i]);
			}
		}
	}

}

DWORD WINAPI EjectVolume(LPVOID lpParam)
{
	TCHAR driveLetter = (TCHAR)lpParam;
	//_tprintf(TEXT("EjectVolume: %c\n"), driveLetter);

	BOOL ret = EjectDriveParrentDeviceByDriveLetter(driveLetter);
	return 0;
}

bool IsValidDriveLetter(TCHAR driveLetter)
{
	if ((driveLetter >= TEXT('A') && driveLetter <= TEXT('Z')) || (driveLetter >= TEXT('a') && driveLetter <= TEXT('z')))
	{
		return true;
	}
	else
	{
		return false;
	}
}

BOOL EjectDriveParrentDeviceByDriveLetter(TCHAR driveLetter)
{
	// It opens the volume and gets its device number
	// than Get Drive's DevInst By sdn_DeviceNumber ,remove the device

	if (!IsValidDriveLetter(driveLetter))
	{
		return false;
	}
	wstring szType = L"UNKNOWN";
	if (IsUDrive(driveLetter)) {
		szType = L"UDrive";
	}
	else if (IsRemovableDrive(driveLetter)) {
		szType = L"RemovableDrive";
	}
	else if (IsRemovableCDRom(driveLetter))
	{
		szType = L"RemovableCDRom";
	}
	else
	{
		return false;
	}

	TCHAR tszSymbol[MAX_PATH] = { TEXT("\\\\?\\*:") };
	tszSymbol[4] = driveLetter;

	BOOL bRet = FALSE;
	DWORD sdn_DeviceNumber = 0;
	DEVICE_TYPE sdn_DeviceType = 0;

	CONFIGRET configRet = CR_SUCCESS;
	DWORD lastErr = 0;
	HANDLE hDevice = CreateFile(tszSymbol, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		lastErr = GetLastError();
		return false;
	}

	STORAGE_DEVICE_NUMBER sdn;
	DWORD dwBytesReturned = 0;
	DWORD dwIoControlCode = IOCTL_STORAGE_GET_DEVICE_NUMBER;
	bRet = DeviceIoControl(hDevice,
		dwIoControlCode,
		NULL, 0, &sdn, sizeof(sdn),
		&dwBytesReturned, NULL);

	CloseHandle(hDevice);
	if (!bRet)
	{
		return false;
	}
	sdn_DeviceType = sdn.DeviceType;
	sdn_DeviceNumber = sdn.DeviceNumber;

	if (FILE_DEVICE_DISK == sdn_DeviceType
		|| FILE_DEVICE_CD_ROM == sdn_DeviceType)
	{
		;
	}
	else 
	{
		return false;
	}

	DEVINST DevInst = GetDrivesDevInstByDeviceNumber_DeviceType(sdn_DeviceNumber, sdn_DeviceType, driveLetter);
	if (DevInst == NULL)
	{
		return false;
	}

	ULONG Status = 0;
	ULONG ProblemNumber = 0;
	PNP_VETO_TYPE VetoType = PNP_VetoTypeUnknown;
	WCHAR VetoNameW[MAX_PATH] = { 0 };
	// get drives's parent, e.g. the USB bridge,
	// the SATA port, an IDE channel with two drives!
	bRet = FALSE;
	DEVINST DevInstParent = 0;
	//obtains a device instance handle to the parent node of a specified device node (devnode) in the local machine's device tree.
	configRet = CM_Get_Parent(&DevInstParent, DevInst, 0);
	if (CR_SUCCESS != configRet)
	{
		bRet = FALSE;
	}
	else
	{
		//get Parent device id
		wstring csParentDeviceID;
		if (GetDeviceIDByDevInst(DevInstParent, csParentDeviceID))
		{
			if (InWhiteIDList(csParentDeviceID))
			{
				_tprintf(TEXT("keep %c: %s\n"), driveLetter, csParentDeviceID.c_str());
			}
			else
			{
				int MAX_TRY_TIMES = 20;
				for (int tries = 0; tries < MAX_TRY_TIMES; tries++)
				{
					// sometimes we need some tries...

					VetoNameW[0] = 0;

					// prepares a local device instance for safe removal, if the device is removable. 
					// If the device can be physically ejected, it will be.
					configRet = CM_Request_Device_EjectW(DevInstParent,
						&VetoType,
						VetoNameW,
						MAX_PATH, 0);

					if (configRet == CR_SUCCESS && VetoType == PNP_VetoTypeUnknown)
					{
						_tprintf(TEXT("-->eject %c: OK\n"), driveLetter);
						bRet = TRUE;
						break;
					}
					Sleep(50); // required to give the next tries a chance!
				}
				if (!bRet) 
				{
					_tprintf(TEXT("-->eject %c: NG\n"), driveLetter);
				}
			}
		}
	}

	return bRet;
}

DEVINST GetDrivesDevInstByDeviceNumber_DeviceType(DWORD DeviceNumber, DEVICE_TYPE DeviceType, TCHAR driveLetter)
{
	GUID guid;
	if (IsUDrive(driveLetter) || IsRemovableDrive(driveLetter)) 
	{
		guid = GUID_DEVINTERFACE_DISK;
	}
	else if (IsRemovableCDRom(driveLetter)) {
		guid = GUID_DEVINTERFACE_CDROM;
	}
	else {
		return NULL;
	}

	// Get device interface info set handle for all devices attached to system
	HDEVINFO hDeviceInfoSet = SetupDiGetClassDevs(&guid, NULL, NULL,
		DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if (hDeviceInfoSet == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	// Retrieve a context structure for a device interface  of a device information set.
	DWORD dwIndex = 0;
	SP_DEVICE_INTERFACE_DATA DeviceInterfaceData = { 0 };
	DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	BOOL bRet = FALSE;

	BYTE Buf[1024] = { 0 };
	PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)Buf;
	SP_DEVICE_INTERFACE_DATA         spdid;
	SP_DEVINFO_DATA                  DeviceInfoData;
	DWORD                            dwRequiredSize;

	spdid.cbSize = sizeof(spdid);

	while (true)
	{
		//enumerates the device interfaces that are contained in a device information set
		bRet = SetupDiEnumDeviceInterfaces(hDeviceInfoSet, NULL, &guid,
			dwIndex, &DeviceInterfaceData);
		if (!bRet)
		{
			break;
		}

		bRet = SetupDiEnumInterfaceDevice(hDeviceInfoSet, NULL, &guid,
			dwIndex, &spdid);

		dwRequiredSize = 0;
		bRet = SetupDiGetDeviceInterfaceDetail(hDeviceInfoSet,
			&spdid, NULL, 0, &dwRequiredSize, NULL);

		if (!bRet && GetLastError() == ERROR_INSUFFICIENT_BUFFER && dwRequiredSize <= sizeof(Buf))
		{
			//pDeviceInterfaceDetailData->cbSize = sizeof(*pDeviceInterfaceDetailData);
			pDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			ZeroMemory((PVOID)&DeviceInfoData, sizeof(DeviceInfoData));
			//DeviceInfoData.cbSize = sizeof(DeviceInfoData);
			DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

			bRet = SetupDiGetDeviceInterfaceDetail(hDeviceInfoSet,
				&spdid, pDeviceInterfaceDetailData, dwRequiredSize, &dwRequiredSize, &DeviceInfoData);
			if (bRet)
			{
				HANDLE hDevice = CreateFile(pDeviceInterfaceDetailData->DevicePath, 0,
					FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
				if (hDevice != INVALID_HANDLE_VALUE)
				{
					STORAGE_DEVICE_NUMBER sdn;
					DWORD dwBytesReturned = 0;
					DWORD dwIoControlCode = IOCTL_STORAGE_GET_DEVICE_NUMBER;
					bRet = DeviceIoControl(hDevice,
						dwIoControlCode,
						NULL, 0, &sdn, sizeof(sdn),
						&dwBytesReturned, NULL);
					if (bRet)
					{
						if (DeviceNumber == sdn.DeviceNumber && DeviceType == sdn.DeviceType)
						{
							CloseHandle(hDevice);
							SetupDiDestroyDeviceInfoList(hDeviceInfoSet);
							return DeviceInfoData.DevInst; //An opaque handle to the device instance (also known as a handle to the devnode).
						}
					}
					CloseHandle(hDevice);
				}
			}
		}
		dwIndex++;
	}

	SetupDiDestroyDeviceInfoList(hDeviceInfoSet);

	return NULL;
}

ATOM MyRegisterClass()
{
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = CLASS_NAME;
	return RegisterClass(&wc);
}

bool CreateMessageOnlyWindow()
{
	g_hWnd = CreateWindowEx(0, CLASS_NAME, _T(""), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL,       // Parent window    
		NULL,       // Menu
		GetModuleHandle(NULL),  // Instance handle
		NULL        // Additional application data
	);

	return g_hWnd != NULL;
}

void RegisterDeviceNotify()
{
	for (int i = 0; i < sizeof(GUID_DEVINTERFACE_LIST) / sizeof(GUID); i++)
	{
		DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
		ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
		NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
		NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
		NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_LIST[i];
		g_NotifyDevHandle = RegisterDeviceNotification(g_hWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
	}
}

int StartApp(const TCHAR* configFile)
{
	//程序单例模式
	HANDLE hMutex = CreateMutex(NULL, true, TEXT("USB BLOCK @RITS V2021.0.1"));
	DWORD err = GetLastError();
	if (err == ERROR_ALREADY_EXISTS)
	{
		//程序已打开，此程序实例退出
		CloseHandle(hMutex);
		exit(0);
	}

	if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleHandler, TRUE) == FALSE)
	{
		// unable to install handler... 
		// display message to the user
		_tprintf(TEXT("Unable to install handler!\n"));
		//return -1;
	}

	_tprintf(TEXT("app start... (press ctrl+break for quit)\n"));

	InitWhiteIDList(configFile);

	EjectSystemInvalidUSBDisk();

	// 创建window，接受系统消息
	g_WindowThreadId = 0;
	g_WindowThreadHandle = CreateThread(NULL, 0, ThreadFunc_Window, NULL, 0, &g_WindowThreadId);
	if (g_WindowThreadHandle == NULL) {
		_tprintf(TEXT("error\n"));
		return -1;
	}
#if 0
	{
		//get window's thread id & process id
		while (g_hWnd == NULL) {
			;
		}
		DWORD dwProcessId = 0;
		DWORD   dwThreadID = GetWindowThreadProcessId(g_hWnd, &dwProcessId);

	}
#endif

	// 等待window 结束
	WaitForSingleObject(g_WindowThreadHandle, INFINITE);
	CloseHandle(g_WindowThreadHandle);
	CloseHandle(hMutex);
	_tprintf(TEXT("app end...\n"));
	return 0;
}

void ShowHelp()
{
	_tprintf(TEXT("usbblock [/L [outfile]] [/?] [infile]\n"));
	_tprintf(TEXT("/L      print USB storage device ID to console or write to outfile.\n"));
	_tprintf(TEXT("infile  load white USB storage device ID from infile.\n"));
	_tprintf(TEXT("/?      show thie help\n"));
}

void ListSystemUSBDiskDeviceID(const TCHAR* pOutFile)
{
/* 
显示当前系统的usb存储设备的盘符 和 Device ID,格式如下：
;F:
USB\VID_125F&PID_312B\1108170000000028
*/
	wofstream hWriteFile;
	if (pOutFile) {
		
		hWriteFile.open(pOutFile, wios::out | wios::trunc);
	}

	if (hWriteFile.is_open())
	{
		hWriteFile << ";Drive:USBID" << endl;
	}
	else 
	{
		_tprintf(TEXT(";Drive:USBID\n"));
	}

	TCHAR driveLetters[26] = { 0 };
	TCHAR driveLetter;
	wstring csDeviceID;

	int cnt = GetUSBDriveLetters(driveLetters, 26);
	for (int i = 0; i < cnt; i++) {
		driveLetter = driveLetters[i];
		if (GetDriveDeviceIDByDriveLetter(driveLetter, csDeviceID)) {
			if (hWriteFile.is_open())
			{
				hWriteFile << ";" << driveLetter << ":" << endl << csDeviceID << endl;
			}
			else 
			{
				_tprintf(TEXT(";%c:\n%s\n"), driveLetter, csDeviceID.c_str());
			}
		}
		else {
			if (hWriteFile.is_open())
			{
				hWriteFile << ";" << driveLetter << ":" << endl << ";ERROR" << endl;
			}
			else 
			{
				_tprintf(TEXT(";%c:\n%s\n"), driveLetter, TEXT(";get id error"));
			}
		}
	}

	if (hWriteFile.is_open())
	{
		hWriteFile.close();
	}

}

void EjectSystemInvalidUSBDisk() 
{
	TCHAR driveLetters[26] = { 0 };
	TCHAR driveLetter;
	wstring csDeviceID;

	int cnt = GetUSBDriveLetters(driveLetters, 26);
	BOOL ret = false;
	for (int i = 0; i < cnt; i++) {
		driveLetter = driveLetters[i];
		if (GetDriveDeviceIDByDriveLetter(driveLetter, csDeviceID)) {
			if (InWhiteIDList(csDeviceID))
			{
				_tprintf(TEXT("keep %c: %s\n"), driveLetter, csDeviceID.c_str());
				continue;
			}
			else
			{
				ret = EjectDriveParrentDeviceByDriveLetter(driveLetter);
			}
		}
	}
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	//get app path
	wstring exePath = argv[0];
	g_AppPath = exePath.substr(0, exePath.find_last_of(TEXT('\\')));

	int paramCnt = argc - 1;
	if (paramCnt == 0) //无参数
	{
		return StartApp(NULL);
	}
	else//参数>=1
	{
		wstring strParam1; //参数1
		strParam1.assign(argv[1]);
		trim(strParam1);
		//help
		if (0 == lstrcmpi(strParam1.c_str(), CMD_HELP)) {//lstrcmpi 不区分大小写比较两个字符串
			ShowHelp();
			return 0;
		}
		//ListSystemUSBDiskDeviceID
		else if (0 == lstrcmpi(strParam1.c_str(), CMD_LISTID))
		{
			if (paramCnt > 1) 
			{
				ListSystemUSBDiskDeviceID(argv[2]); //输出deviceid到文件
			}
			else 
			{
				ListSystemUSBDiskDeviceID(NULL);  //输出deviceid到console
			}
			return 0;
		}
		else
		{
			return StartApp(strParam1.c_str());
		}
	}
}