/*
 * PROJECT:         Aspia (Info Helper DLL)
 * FILE:            aspia_dll/services.c
 * LICENSE:         LGPL (GNU Lesser General Public License)
 * PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
 */

#include "aspia.h"
#include "aspia_dll.h"


ENUM_SERVICE_STATUS_PROCESS *pAllServices = NULL;


static BOOL
GetServiceList(SIZE_T *NumServices, DWORD dwServiceType)
{
    SC_HANDLE ScHandle;
    BOOL bRet = FALSE;

    DWORD BytesNeeded = 0;
    DWORD ResumeHandle = 0;

    *NumServices = 0;

    ScHandle = OpenSCManager(NULL, NULL,
                             SC_MANAGER_ENUMERATE_SERVICE);
    if (ScHandle != INVALID_HANDLE_VALUE)
    {
        if (!EnumServicesStatusEx(ScHandle,
                                  SC_ENUM_PROCESS_INFO,
                                  dwServiceType,
                                  SERVICE_STATE_ALL,
                                  NULL,
                                  0,
                                  &BytesNeeded,
                                  NumServices,
                                  &ResumeHandle,
                                  0))
        {
            /* Call function again if required size was returned */
            if (GetLastError() == ERROR_MORE_DATA)
            {
                /* reserve memory for service info array */
                pAllServices = (ENUM_SERVICE_STATUS_PROCESS*)Alloc(BytesNeeded);
                if (pAllServices)
                {
                    /* fill array with service info */
                    if (EnumServicesStatusEx(ScHandle,
                                             SC_ENUM_PROCESS_INFO,
                                             dwServiceType,
                                             SERVICE_STATE_ALL,
                                             (LPBYTE)pAllServices,
                                             BytesNeeded,
                                             &BytesNeeded,
                                             NumServices,
                                             &ResumeHandle,
                                             0))
                    {
                        bRet = TRUE;
                    }
                }
            }
        }
    }

    if (ScHandle)
        CloseServiceHandle(ScHandle);

    if (!bRet)
        Free(pAllServices);

    return bRet;
}

static LPQUERY_SERVICE_CONFIG
GetServiceConfig(LPWSTR lpServiceName)
{
    LPQUERY_SERVICE_CONFIG lpServiceConfig = NULL;
    SC_HANDLE hSCManager;
    SC_HANDLE hService;
    DWORD dwBytesNeeded;

    hSCManager = OpenSCManager(NULL, NULL,
                               SC_MANAGER_ALL_ACCESS);
    if (hSCManager)
    {
        hService = OpenService(hSCManager,
                               lpServiceName,
                               SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS |
                               SERVICE_QUERY_CONFIG);
        if (hService)
        {
            if (!QueryServiceConfig(hService,
                                    NULL,
                                    0,
                                    &dwBytesNeeded))
            {
                if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                {
                    lpServiceConfig = (LPQUERY_SERVICE_CONFIG)Alloc(dwBytesNeeded);
                    if (lpServiceConfig)
                    {
                        if (!QueryServiceConfig(hService,
                                                lpServiceConfig,
                                                dwBytesNeeded,
                                                &dwBytesNeeded))
                        {
                            Free(lpServiceConfig);
                            lpServiceConfig = NULL;
                        }
                    }
                }
            }

            CloseServiceHandle(hService);
        }

        CloseServiceHandle(hSCManager);
    }

    return lpServiceConfig;
}

static LPWSTR
GetServiceDescription(LPWSTR lpServiceName)
{
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hSc = NULL;
    SERVICE_DESCRIPTION *pServiceDescription = NULL;
    LPWSTR lpDescription = NULL;
    DWORD BytesNeeded = 0;
    DWORD dwSize;

    hSCManager = OpenSCManager(NULL, NULL,
                               SC_MANAGER_ENUMERATE_SERVICE);
    if (!hSCManager)
    {
        return NULL;
    }

    hSc = OpenService(hSCManager,
                      lpServiceName,
                      SERVICE_QUERY_CONFIG);
    if (hSc)
    {
        if (!QueryServiceConfig2(hSc,
                                 SERVICE_CONFIG_DESCRIPTION,
                                 NULL,
                                 0,
                                 &BytesNeeded))
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
            {
                pServiceDescription = (SERVICE_DESCRIPTION*)Alloc(BytesNeeded);
                if (pServiceDescription == NULL)
                    goto cleanup;

                if (QueryServiceConfig2(hSc,
                                        SERVICE_CONFIG_DESCRIPTION,
                                        (LPBYTE)pServiceDescription,
                                        BytesNeeded,
                                        &BytesNeeded))
                {
                    if (pServiceDescription->lpDescription)
                    {
                        dwSize = (DWORD)(SafeStrLen(pServiceDescription->lpDescription) + 1) * sizeof(WCHAR);
                        lpDescription = Alloc(dwSize);
                        if (lpDescription)
                        {
                            StringCbCopy(lpDescription,
                                         dwSize,
                                         pServiceDescription->lpDescription);
                        }
                    }
                }
            }
        }
    }

cleanup:
    if (pServiceDescription)
        Free(pServiceDescription);
    if (hSCManager) CloseServiceHandle(hSCManager);
    if (hSc) CloseServiceHandle(hSc);

    return lpDescription;
}

static VOID
ServiceStatusToText(DWORD dwStatus, LPWSTR lpszText, SIZE_T Size)
{
    UINT uiID;

    switch (dwStatus)
    {
        case SERVICE_CONTINUE_PENDING:
            uiID = IDS_SERVICE_CONT_PENDING;
            break;
        case SERVICE_PAUSE_PENDING:
            uiID = IDS_SERVICE_PAUSE_PENDING;
            break;
        case SERVICE_PAUSED:
            uiID = IDS_SERVICE_PAUSED;
            break;
        case SERVICE_RUNNING:
            uiID = IDS_SERVICE_RUNNING;
            break;
        case SERVICE_START_PENDING:
            uiID = IDS_SERVICE_START_PENDING;
            break;
        case SERVICE_STOP_PENDING:
            uiID = IDS_SERVICE_STOP_PENDING;
            break;
        case SERVICE_STOPPED:
            uiID = IDS_SERVICE_STOPPED;
            break;
        default:
            StringCbCopy(lpszText, Size, L"Unknown");
            return;
    }

    LoadMUIString(uiID, lpszText, Size);
}

static VOID
ServiceStartTypeToText(DWORD dwStartType, LPWSTR lpszText, SIZE_T Size)
{
    UINT uiID;

    switch (dwStartType)
    {
        case SERVICE_AUTO_START:
            uiID = IDS_SERVICE_AUTO;
            break;
        case SERVICE_DEMAND_START:
            uiID = IDS_SERVICE_MANUAL;
            break;
        case SERVICE_DISABLED:
            uiID = IDS_SERVICE_DISABLED;
            break;
        case SERVICE_BOOT_START:
            uiID = IDS_SERVICE_BOOT_START;
            break;
        case SERVICE_SYSTEM_START:
            uiID = IDS_SERVICE_SYSTEM_START;
            break;
        default:
            StringCbCopy(lpszText, Size, L"Unknown");
            return;
    }

    LoadMUIString(uiID, lpszText, Size);
}

VOID
SOFTWARE_ServicesInfo(VOID)
{
    ENUM_SERVICE_STATUS_PROCESS *pService = NULL;
    LPQUERY_SERVICE_CONFIG lpServiceConfig = NULL;
    LPWSTR lpDescription = NULL;
    WCHAR szText[MAX_STR_LEN];
    SIZE_T NumServices;
    SIZE_T Index;

    DebugStartReceiving();

    IoAddIcon(IDI_SERVICES);

    if (GetServiceList(&NumServices, SERVICE_WIN32))
    {
        for (Index = 0; Index < NumServices; ++Index)
        {
            pService = &pAllServices[Index];
            if (!pService) return;

            /* Display name */
            IoAddItem(0, 0, pService->lpDisplayName);

            /* Name */
            IoSetItemText(pService->lpServiceName);

            /* Description */
            lpDescription = GetServiceDescription(pService->lpServiceName);
            if (lpDescription)
            {
                IoSetItemText(lpDescription);
                Free(lpDescription);
            }
            else
            {
                IoSetItemText(L"-");
            }

            /* Status */
            ServiceStatusToText(pService->ServiceStatusProcess.dwCurrentState,
                                szText, MAX_STR_LEN);
            IoSetItemText(szText);

            lpServiceConfig = GetServiceConfig(pService->lpServiceName);
            if (lpServiceConfig)
            {
                /* Startup type */
                ServiceStartTypeToText(lpServiceConfig->dwStartType,
                                       szText, MAX_STR_LEN);
                IoSetItemText(szText);
                /* Start name */
                IoSetItemText((SafeStrLen(lpServiceConfig->lpServiceStartName) > 3) ?
                        lpServiceConfig->lpServiceStartName : L"-");
                /* Binary path name */
                IoSetItemText((SafeStrLen(lpServiceConfig->lpBinaryPathName) > 3) ?
                        lpServiceConfig->lpBinaryPathName : L"-");

                Free(lpServiceConfig);
            }
            else
            {
                IoSetItemText(L"-");
                IoSetItemText(L"-");
                IoSetItemText(L"-");
            }

            if (IsCanceled) break;
        }

        Free(pAllServices);
    }

    DebugEndReceiving();
}

BOOL
IsImagePathExists(LPWSTR lpName)
{
    DWORD dwType, dwSize;
    WCHAR szPath[MAX_PATH], szImg[MAX_PATH];
    HKEY hKey;

    StringCbPrintf(szPath, sizeof(szPath),
                   L"SYSTEM\\CurrentControlSet\\services\\%s",
                   lpName);

    if (RegOpenKey(HKEY_LOCAL_MACHINE,
                   szPath,
                   &hKey) != ERROR_SUCCESS)
    {
        return FALSE;
    }

    dwType = REG_SZ;
    dwSize = MAX_PATH;

    if (RegQueryValueEx(hKey,
                        L"ImagePath",
                        NULL,
                        &dwType,
                        (LPBYTE)szImg,
                        &dwSize) == ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return TRUE;
    }

    RegCloseKey(hKey);

    return FALSE;
}

VOID
SOFTWARE_DriversInfo(VOID)
{
    ENUM_SERVICE_STATUS_PROCESS *pService = NULL;
    LPQUERY_SERVICE_CONFIG lpServiceConfig = NULL;
    LPWSTR lpDescription = NULL;
    WCHAR szText[MAX_STR_LEN];
    SIZE_T NumServices;
    SIZE_T Index;

    DebugStartReceiving();

    IoAddIcon(IDI_HW);

    if (GetServiceList(&NumServices, SERVICE_DRIVER))
    {
        for (Index = 0; Index < NumServices; ++Index)
        {
            pService = &pAllServices[Index];
            if (!pService) return;

            /* Display name */
            IoAddItem(0, 0, pService->lpDisplayName);

            /* Name */
            IoSetItemText(pService->lpServiceName);

            /* Description */
            lpDescription = GetServiceDescription(pService->lpServiceName);
            if (lpDescription)
            {
                IoSetItemText(lpDescription);
                Free(lpDescription);
            }
            else
            {
                IoSetItemText(L"-");
            }

            /* Status */
            ServiceStatusToText(pService->ServiceStatusProcess.dwCurrentState,
                                szText, MAX_STR_LEN);
            IoSetItemText(szText);

            lpServiceConfig = GetServiceConfig(pService->lpServiceName);
            if (lpServiceConfig)
            {
                /* Startup type */
                ServiceStartTypeToText(lpServiceConfig->dwStartType,
                                       szText, MAX_STR_LEN);
                IoSetItemText(szText);

                /* Binary path name */
                if (IsImagePathExists(pService->lpServiceName))
                {
                    IoSetItemText(lpServiceConfig->lpBinaryPathName);
                }
                else
                {
                    IoSetItemText(L"-");
                }

                Free(lpServiceConfig);
            }
            else
            {
                IoSetItemText(L"-");
                IoSetItemText(L"-");
            }

            if (IsCanceled) break;
        }

        Free(pAllServices);
    }

    DebugEndReceiving();
}