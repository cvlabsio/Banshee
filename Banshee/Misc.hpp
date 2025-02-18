#pragma once

#include <ntifs.h>
#include "DriverMeta.hpp"

#define DRIVER_LOG_PREFIX "::[Banshee] - "
#define LOG_MSG(x, ...) DbgPrint((DRIVER_LOG_PREFIX x), __VA_ARGS__)

/*
 * Check whether a wstring is a nullpointer or contains only null characters
 *
 * @return BOOLEAN True if nullpointer or only null characters
 */
BOOLEAN
BeIsStringNull(PWCHAR pWchar)
{
    return (pWchar == NULL || *pWchar == '\0');
}

/**
 * Check whether a wstring is null-terminated
 *
 * @return BOOLEAN True if null-terminated.
 */
BOOLEAN
BeIsStringTerminated(PWCHAR Array, ULONG ArrayLength)
{
    BOOLEAN bStringIsTerminated = FALSE;
    USHORT uiIndex = 0;

    while (uiIndex < ArrayLength && bStringIsTerminated == FALSE)
    {
        if (Array[uiIndex] == L'\0')
        {
            bStringIsTerminated = TRUE;
        }
        else
        {
            uiIndex++;
        }
    }
    return bStringIsTerminated;
}

/**
 * Check whether a wstring is null-terminated, not empty and properly aligned
 *
 * @return NTSTATUS depending on if the checks succeed or not
 */
NTSTATUS
BeCheckStringIsAlignedNotEmptyAndTerminated(PWCHAR targetString, ULONG dwSize)
{
    // Check alignment
    if (dwSize % sizeof(WCHAR) != 0)
    {
        LOG_MSG("Invalid alignment \r\n");
        return STATUS_INVALID_BUFFER_SIZE;
    }

    if (!targetString)
    {
        LOG_MSG("Empty buffer \r\n");
        return STATUS_INVALID_PARAMETER;
    }

    LOG_MSG("String received: %ws \r\n", targetString);

    if (BeIsStringTerminated(targetString, dwSize) == FALSE)
    {
        LOG_MSG("Not null terminated! \r\n");
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

/*
 * compares two wchar strings without case sensitivity
 * 
 * @param s1 first string
 * @param s2 second string 
 * @return INT 0 if both string are qual
 */
INT
_strcmpi_w(const wchar_t* s1, const wchar_t* s2)
{
    WCHAR c1, c2;

    if (s1 == s2)
        return 0;

    if (s1 == 0)
        return -1;

    if (s2 == 0)
        return 1;

    do {
        c1 = RtlUpcaseUnicodeChar(*s1);
        c2 = RtlUpcaseUnicodeChar(*s2);
        s1++;
        s2++;
    } while ((c1 != 0) && (c1 == c2));

    return (INT)(c1 - c2);
}

/*
 * check if a wide string contains a substring without case sensitivity
 * 
 * @param String string to check
 * @param Pattern pattern to look for
 * @return PWCHAR NULL if no match is found, otherwise a pointer to the match
 */
PWCHAR
StrStrIW(const PWCHAR String, const PWCHAR Pattern)
{
      PWCHAR pptr, sptr, start;

      for (start = (PWCHAR)String; *start != NULL; ++start)
      {
            while (((*start!=NULL) && (RtlUpcaseUnicodeChar(*start) != RtlUpcaseUnicodeChar(*Pattern))))
            {
                ++start;
            }

            if (NULL == *start)
                  return NULL;

            pptr = (PWCHAR)Pattern;
            sptr = (PWCHAR)start;

            while (RtlUpcaseUnicodeChar(*sptr) == RtlUpcaseUnicodeChar(*pptr))
            {
                  sptr++;
                  pptr++;

                  if (NULL == *pptr)
                        return (start);
            }
      }

      return NULL;
}

/*
 * Gets the base name of a full file path
 * Taken from: https://github.com/GetRektBoy724/DCMB/blob/main/DCMB/dcmb.c#L3
 * 
 * @returns PCHAR base name
 */
PCHAR 
GetBaseNameFromFullPath(PCHAR FullName) 
{
    SIZE_T FullNameLength = strlen(FullName);

    for (SIZE_T i = FullNameLength; i > 0; i--) 
    {
        if (*(FullName + i) == '\\') 
        {
            return FullName + i + 1;
        }
    }

    return NULL;
}

/**
 * TODO
 */
NTSTATUS 
BeWaitForEvent(HANDLE EventHandle)
{
    NTSTATUS status;

    // TODO: move this to the global init routine and only do it once but im too tired rn ...
    KEVENT* pEvent = (KEVENT*)ExAllocatePoolWithTag(NonPagedPool, sizeof(KEVENT), DRIVER_TAG);
    if (!pEvent)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeInitializeEvent(pEvent, NotificationEvent, FALSE);

    status = ObReferenceObjectByHandle(EventHandle, EVENT_ALL_ACCESS, *ExEventObjectType, KernelMode, (PVOID*)&pEvent, NULL);
    if (!NT_SUCCESS(status))
    {
        ExFreePool(pEvent);
        return status;
    }

    status = KeWaitForSingleObject(pEvent, Executive, KernelMode, FALSE, NULL);
    if (!NT_SUCCESS(status))
    {
        ObDereferenceObject(pEvent);
        return status;
    }

    ObDereferenceObject(pEvent);
    ExFreePool(pEvent);

    return STATUS_SUCCESS;
}

/**
 * TODO
 */
NTSTATUS 
BeCreateNamedEvent(PHANDLE phEvent, PUNICODE_STRING EventName)
{
    NTSTATUS status;

    OBJECT_ATTRIBUTES objAttributes;
    InitializeObjectAttributes(&objAttributes, EventName, OBJ_OPENIF | OBJ_CASE_INSENSITIVE | OBJ_PERMANENT | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwCreateEvent(phEvent, EVENT_ALL_ACCESS, &objAttributes, NotificationEvent, FALSE);

    if (!NT_SUCCESS(status)) 
    {
        DbgPrint("Failed to create named event: 0x%X\n", status);
    }

    return status;
}

/**
 * TODO
 */
NTSTATUS 
BeSetNamedEvent(HANDLE hEvent, BOOLEAN set)
{
    NTSTATUS status = STATUS_SUCCESS;

    // TODO: move this to the global init routine and only do it once but im too tired rn ...
    KEVENT* pEvent = (KEVENT*)ExAllocatePoolWithTag(NonPagedPool, sizeof(KEVENT), DRIVER_TAG);
    if (!pEvent)
    {
        LOG_MSG("Failed to ExAllocatePoolWithTag event object\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeInitializeEvent(pEvent, NotificationEvent, FALSE);

    status = ObReferenceObjectByHandle(hEvent, SYNCHRONIZE, *ExEventObjectType, KernelMode, (PVOID*)&pEvent, NULL);
    if (!NT_SUCCESS(status))
    {
        LOG_MSG("Failed to obtain event object: 0x%X\n", status);
        return status;
    }

    if (set)
    {
        status = KeSetEvent(pEvent, IO_NO_INCREMENT, FALSE);
        if (!NT_SUCCESS(status))
        {
            LOG_MSG("Failed to set named event: 0x%X\n", status);
        }
    }
    else
    {
        status = KeResetEvent(pEvent);
        if (!NT_SUCCESS(status))
        {
            LOG_MSG("Failed to reset named event: 0x%X\n", status);
        }
    }

    if (pEvent)
    {
        ObDereferenceObject(pEvent);
    }

    return status;
}