#pragma once

#include "stdafx.hpp"



typedef struct _EPROCESS
{
	char		Padding[0x440];
	ULONG64		UniqueProcessId;
	LIST_ENTRY	ActiveProcessLinks;
}_EPROCESS;