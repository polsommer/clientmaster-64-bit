#if !defined(SWG_DIRECT3D9EX_SUPPORT_HEADER_ONLY)
#define SWG_DIRECT3D9EX_SUPPORT_SOURCE 1
#include "Direct3d9ExSupport.h"

namespace Direct3d9ExSupport
{
        namespace
        {
                bool isValidHandle(HMODULE module)
                {
                        return module != NULL && module != reinterpret_cast<HMODULE>(INVALID_HANDLE_VALUE);
                }

                PFN_Direct3DCreate9Ex resolveCreateProc(HMODULE module)
                {
                        return module ? reinterpret_cast<PFN_Direct3DCreate9Ex>(GetProcAddress(module, "Direct3DCreate9Ex")) : NULL;
                }
        }

        RuntimeHandles::RuntimeHandles()
        : module(NULL)
        , loaded(false)
        , createProc(NULL)
        {
        }

        HMODULE loadRuntime(bool loadIfMissing, bool *outLoaded)
        {
                if (outLoaded)
                        *outLoaded = false;

                HMODULE module = GetModuleHandle(TEXT("d3d9.dll"));
                if (!isValidHandle(module) && loadIfMissing)
                {
                        module = LoadLibrary(TEXT("d3d9.dll"));
                        if (outLoaded)
                                *outLoaded = isValidHandle(module);
                }

                return isValidHandle(module) ? module : NULL;
        }

        void unloadRuntime(HMODULE module, bool loaded)
        {
                if (loaded && isValidHandle(module))
                        FreeLibrary(module);
        }

        PFN_Direct3DCreate9Ex getCreate9ExProc(HMODULE module)
        {
                return resolveCreateProc(module);
        }

        HRESULT createInterface(PFN_Direct3DCreate9Ex createProc, UINT sdkVersion, IDirect3D9Ex **outInterface)
        {
                if (!outInterface)
                        return E_POINTER;

                *outInterface = NULL;
                if (!createProc)
                        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

                return createProc(sdkVersion, outInterface);
        }

        HRESULT createInterface(UINT sdkVersion, IDirect3D9Ex **outInterface)
        {
                bool loaded = false;
                HMODULE module = loadRuntime(true, &loaded);
                if (!module)
                        return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);

                PFN_Direct3DCreate9Ex const proc = resolveCreateProc(module);
                HRESULT const result = createInterface(proc, sdkVersion, outInterface);

                if (FAILED(result) && loaded)
                        unloadRuntime(module, true);

                return result;
        }

        bool acquireRuntime(bool loadIfMissing, RuntimeHandles *outHandles)
        {
                if (!outHandles)
                        return false;

                RuntimeHandles handles;
                handles.module = loadRuntime(loadIfMissing, &handles.loaded);
                if (!handles.module)
                {
                        *outHandles = handles;
                        return false;
                }

                handles.createProc = resolveCreateProc(handles.module);
                *outHandles = handles;
                return handles.createProc != NULL;
        }

        void releaseRuntime(RuntimeHandles &handles)
        {
                if (handles.module)
                {
                        unloadRuntime(handles.module, handles.loaded);
                        handles.module = NULL;
                }

                handles.loaded = false;
                handles.createProc = NULL;
        }

        HRESULT createInterface(const RuntimeHandles &handles, UINT sdkVersion, IDirect3D9Ex **outInterface)
        {
                return createInterface(handles.createProc, sdkVersion, outInterface);
        }

        bool isRuntimeAvailable()
        {
                HMODULE module = loadRuntime(false, NULL);
                if (!module)
                        return false;

                return resolveCreateProc(module) != NULL;
        }

        bool isDeviceRemovedError(HRESULT result)
        {
                return result == D3DERR_DEVICEREMOVED
                        || result == D3DERR_DEVICEHUNG
                        || result == D3DERR_DEVICELOST
                        || result == D3DERR_DRIVERINTERNALERROR;
        }

        UINT clampMaximumFrameLatency(UINT latency)
        {
                if (latency < 1u)
                        latency = 1u;
                if (latency > 16u)
                        latency = 16u;
                return latency;
        }

        INT clampGpuThreadPriority(INT priority)
        {
                if (priority < -7)
                        priority = -7;
                if (priority > 7)
                        priority = 7;
                return priority;
        }

        HRESULT setMaximumFrameLatency(IDirect3DDevice9Ex *deviceEx, UINT latency)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->SetMaximumFrameLatency(clampMaximumFrameLatency(latency));
        }

        HRESULT setGpuThreadPriority(IDirect3DDevice9Ex *deviceEx, INT priority)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->SetGPUThreadPriority(clampGpuThreadPriority(priority));
        }

        HRESULT getGpuThreadPriority(IDirect3DDevice9Ex *deviceEx, INT *outPriority)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->GetGPUThreadPriority(outPriority);
        }

        HRESULT waitForVBlank(IDirect3DDevice9Ex *deviceEx, UINT adapter)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->WaitForVBlank(adapter);
        }

        char const *describeDeviceRemovedReason(HRESULT result)
        {
                switch (result)
                {
                        case D3DERR_DEVICEREMOVED:
                                return "D3DERR_DEVICEREMOVED";
                        case D3DERR_DEVICEHUNG:
                                return "D3DERR_DEVICEHUNG";
                        case D3DERR_DEVICELOST:
                                return "D3DERR_DEVICELOST";
                        case D3DERR_DRIVERINTERNALERROR:
                                return "D3DERR_DRIVERINTERNALERROR";
                        default:
                                return "UNKNOWN";
                }
        }
}
#endif // !defined(SWG_DIRECT3D9EX_SUPPORT_HEADER_ONLY)
