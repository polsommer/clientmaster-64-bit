// ======================================================================
//
// Direct3d9ExSupport.h
// Helper declarations to enable Direct3D 9Ex support when building
// against legacy DirectX 9 headers.  In addition to the interface shims
// the header now exposes a small utility namespace that dynamically
// resolves the 9Ex entry points at runtime.  The utility namespace has
// been extended to provide reusable helpers that can be exported from a
// shared library when integrating with tooling outside of the classic
// client tree.
//
// ======================================================================

#ifndef INCLUDED_Direct3d9ExSupport_H
#define INCLUDED_Direct3d9ExSupport_H

#if !defined(SWG_DIRECT3D9EX_SUPPORT_HEADER_ONLY)
#if !defined(SWG_DIRECT3D9EX_SUPPORT_DLL) && !defined(SWG_DIRECT3D9EX_SUPPORT_SOURCE)
#define SWG_DIRECT3D9EX_SUPPORT_HEADER_ONLY 1
#endif
#endif

#if defined(_WIN32) && defined(SWG_DIRECT3D9EX_SUPPORT_DLL)
#ifdef SWG_DIRECT3D9EX_SUPPORT_BUILD
#define DIRECT3D9EX_API __declspec(dllexport)
#else
#define DIRECT3D9EX_API __declspec(dllimport)
#endif // SWG_DIRECT3D9EX_SUPPORT_BUILD
#else
#define DIRECT3D9EX_API
#endif // defined(_WIN32) && defined(SWG_DIRECT3D9EX_SUPPORT_DLL)

#ifndef DIRECT3D_VERSION
#include <d3d9.h>
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#if defined(D3DERR_DEVICEHUNG)
#define DIRECT3D9EX_SUPPORT_NEEDS_LEGACY_DECLS 0
#else
#define DIRECT3D9EX_SUPPORT_NEEDS_LEGACY_DECLS 1
#endif

#ifndef D3DERR_DEVICEREMOVED
#define D3DERR_DEVICEREMOVED MAKE_D3DHRESULT(2160)
#endif

#ifndef D3DERR_DEVICEHUNG
#define D3DERR_DEVICEHUNG MAKE_D3DHRESULT(2164)
#endif

#ifndef S_PRESENT_OCCLUDED
#define S_PRESENT_OCCLUDED MAKE_D3DSTATUS(2162)
#endif

#if DIRECT3D9EX_SUPPORT_NEEDS_LEGACY_DECLS

#ifndef D3DSCANLINEORDERING_DEFINED
#define D3DSCANLINEORDERING_DEFINED
typedef enum _D3DSCANLINEORDERING
{
        D3DSCANLINEORDERING_UNKNOWN      = 0,
        D3DSCANLINEORDERING_PROGRESSIVE  = 1,
        D3DSCANLINEORDERING_INTERLACED   = 2,
        D3DSCANLINEORDERING_FORCE_DWORD  = 0x7fffffff
} D3DSCANLINEORDERING;
#endif

#ifndef D3DCOMPOSERECTSOP_DEFINED
#define D3DCOMPOSERECTSOP_DEFINED
typedef enum _D3DCOMPOSERECTSOP
{
        D3DCOMPOSERECTS_COPY             = 1,
        D3DCOMPOSERECTS_OR               = 2,
        D3DCOMPOSERECTS_AND              = 3,
        D3DCOMPOSERECTS_NEG              = 4,
        D3DCOMPOSERECTS_FORCE_DWORD      = 0x7fffffff
} D3DCOMPOSERECTSOP;
#endif

#ifndef D3DDISPLAYROTATION_DEFINED
#define D3DDISPLAYROTATION_DEFINED
typedef enum _D3DDISPLAYROTATION
{
        D3DDISPLAYROTATION_IDENTITY      = 1,
        D3DDISPLAYROTATION_90            = 2,
        D3DDISPLAYROTATION_180           = 3,
        D3DDISPLAYROTATION_270           = 4,
        D3DDISPLAYROTATION_FORCE_DWORD   = 0x7fffffff
} D3DDISPLAYROTATION;
#endif

#ifndef D3DDISPLAYMODEEX_DEFINED
#define D3DDISPLAYMODEEX_DEFINED
typedef struct _D3DDISPLAYMODEEX
{
        UINT                    Size;
        UINT                    Width;
        UINT                    Height;
        UINT                    RefreshRate;
        D3DFORMAT               Format;
        D3DSCANLINEORDERING     ScanLineOrdering;
} D3DDISPLAYMODEEX;
#endif

#ifndef D3DDISPLAYMODEFILTER_DEFINED
#define D3DDISPLAYMODEFILTER_DEFINED
typedef struct _D3DDISPLAYMODEFILTER
{
        UINT                    Size;
        D3DFORMAT               Format;
        D3DSCANLINEORDERING     ScanLineOrdering;
} D3DDISPLAYMODEFILTER;
#endif

#ifndef D3DPRESENTSTATS_DEFINED
#define D3DPRESENTSTATS_DEFINED
typedef struct _D3DPRESENTSTATS
{
        UINT PresentCount;
        UINT PresentRefreshCount;
        UINT SyncRefreshCount;
        UINT SyncQPCTime;
        UINT SyncGPUTime;
} D3DPRESENTSTATS;
#endif

#ifndef __IDirect3D9Ex_INTERFACE_DEFINED__
#define __IDirect3D9Ex_INTERFACE_DEFINED__
struct IDirect3DDevice9Ex;

struct IDirect3D9Ex : public IDirect3D9
{
        virtual UINT    STDMETHODCALLTYPE GetAdapterModeCountEx(UINT adapter, const D3DDISPLAYMODEFILTER *filter) = 0;
        virtual HRESULT STDMETHODCALLTYPE EnumAdapterModesEx(UINT adapter, const D3DDISPLAYMODEFILTER *filter, UINT mode, D3DDISPLAYMODEEX *displayMode) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetAdapterDisplayModeEx(UINT adapter, D3DDISPLAYMODEEX *displayMode, D3DDISPLAYROTATION *rotation) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateDeviceEx(UINT adapter, D3DDEVTYPE deviceType, HWND focusWindow, DWORD behaviorFlags, D3DPRESENT_PARAMETERS *presentationParameters, D3DDISPLAYMODEEX *fullscreenDisplayMode, IDirect3DDevice9Ex **returnedDeviceInterface) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetAdapterLUID(UINT adapter, LUID *luid) = 0;
};
#endif

#ifndef __IDirect3DDevice9Ex_INTERFACE_DEFINED__
#define __IDirect3DDevice9Ex_INTERFACE_DEFINED__
struct IDirect3DDevice9Ex : public IDirect3DDevice9
{
        virtual HRESULT STDMETHODCALLTYPE SetConvolutionMonoKernel(UINT width, UINT height, float *rowWeights, float *columnWeights) = 0;
        virtual HRESULT STDMETHODCALLTYPE ComposeRects(IDirect3DSurface9 *source, IDirect3DSurface9 *destination, IDirect3DVertexBuffer9 *srcRectDescriptors, UINT rectDescCount, IDirect3DSurface9 *dstRectDescriptors, D3DCOMPOSERECTSOP operation, int xoffset, int yoffset) = 0;
        virtual HRESULT STDMETHODCALLTYPE PresentEx(const RECT *sourceRect, const RECT *destRect, HWND destWindowOverride, const RGNDATA *dirtyRegion, DWORD flags) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetGPUThreadPriority(INT *priority) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetGPUThreadPriority(INT priority) = 0;
        virtual HRESULT STDMETHODCALLTYPE WaitForVBlank(UINT adapter) = 0;
        virtual HRESULT STDMETHODCALLTYPE CheckResourceResidency(IDirect3DResource9 **resourceArray, UINT resourceCount) = 0;
        virtual HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT maxLatency) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT *maxLatency) = 0;
        virtual HRESULT STDMETHODCALLTYPE CheckDeviceState(HWND destWindow) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetEx(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL lockable, IDirect3DSurface9 **surface, DWORD usage) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateOffscreenPlainSurfaceEx(UINT width, UINT height, D3DFORMAT format, D3DPOOL pool, IDirect3DSurface9 **surface, DWORD usage) = 0;
        virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilSurfaceEx(UINT width, UINT height, D3DFORMAT format, D3DMULTISAMPLE_TYPE multiSample, DWORD multisampleQuality, BOOL discard, IDirect3DSurface9 **surface, DWORD usage) = 0;
        virtual HRESULT STDMETHODCALLTYPE ResetEx(D3DPRESENT_PARAMETERS *presentationParameters, D3DDISPLAYMODEEX *fullscreenDisplayMode) = 0;
        virtual HRESULT STDMETHODCALLTYPE GetDisplayModeEx(UINT swapChain, D3DDISPLAYMODEEX *mode, D3DDISPLAYROTATION *rotation) = 0;
};
#endif

#endif // DIRECT3D9EX_SUPPORT_NEEDS_LEGACY_DECLS

#ifndef PFN_Direct3DCreate9Ex
typedef HRESULT (WINAPI *PFN_Direct3DCreate9Ex)(UINT, IDirect3D9Ex **);
#endif

namespace Direct3d9ExSupport
{
        struct RuntimeHandles
        {
                RuntimeHandles();

                HMODULE                 module;
                bool                    loaded;
                PFN_Direct3DCreate9Ex   createProc;
        };

        // Returns a handle to d3d9.dll. When the library is not yet loaded
        // the helper optionally loads it and reports the action through the
        // outLoaded flag. Passing loadIfMissing=false performs a non-loading
        // probe.
        DIRECT3D9EX_API HMODULE loadRuntime(bool loadIfMissing, bool *outLoaded);

        // Releases the runtime if it was loaded through loadRuntime(). Callers
        // should pass the value returned through outLoaded so that we do not
        // accidentally unload a module owned by the host application.
        DIRECT3D9EX_API void unloadRuntime(HMODULE module, bool loaded);

        // Retrieves the Direct3DCreate9Ex entry point from the supplied module.
        DIRECT3D9EX_API PFN_Direct3DCreate9Ex getCreate9ExProc(HMODULE module);

        // Invokes Direct3DCreate9Ex through the provided function pointer.
        DIRECT3D9EX_API HRESULT createInterface(PFN_Direct3DCreate9Ex createProc, UINT sdkVersion, IDirect3D9Ex **outInterface);

        // Convenience helper that loads the runtime (if necessary) and creates
        // the IDirect3D9Ex interface in one call.
        DIRECT3D9EX_API HRESULT createInterface(UINT sdkVersion, IDirect3D9Ex **outInterface);

        // Aggregates the runtime handle, load-state and resolved entry point in
        // one structure. Returns true when the loader could resolve the
        // Direct3DCreate9Ex entry point.
        DIRECT3D9EX_API bool acquireRuntime(bool loadIfMissing, RuntimeHandles *outHandles);

        // Releases the resources held by RuntimeHandles. The structure remains
        // valid for reuse, but the underlying module handle is reset.
        DIRECT3D9EX_API void releaseRuntime(RuntimeHandles &handles);

        // Creates the IDirect3D9Ex interface using a pre-acquired runtime
        // description. This overload allows callers to control the module
        // lifetime without reloading the runtime for subsequent attempts.
        DIRECT3D9EX_API HRESULT createInterface(const RuntimeHandles &handles, UINT sdkVersion, IDirect3D9Ex **outInterface);

        // Returns true when the host system exposes the Direct3D 9Ex entry
        // point. No additional runtime state is modified.
        DIRECT3D9EX_API bool isRuntimeAvailable();

        // Utility helper that reports whether the supplied HRESULT maps to one
        // of the device removal conditions introduced with the 9Ex runtime.
        DIRECT3D9EX_API bool isDeviceRemovedError(HRESULT result);

        // Clamps the provided frame latency to the range supported by 9Ex.
        DIRECT3D9EX_API UINT clampMaximumFrameLatency(UINT latency);

        // Clamps the GPU thread priority to the supported -7..7 range.
        DIRECT3D9EX_API INT clampGpuThreadPriority(INT priority);

        // Applies the configured maximum frame latency to the supplied device.
        DIRECT3D9EX_API HRESULT setMaximumFrameLatency(IDirect3DDevice9Ex *deviceEx, UINT latency);

        // Updates the GPU thread priority on the supplied device.
        DIRECT3D9EX_API HRESULT setGpuThreadPriority(IDirect3DDevice9Ex *deviceEx, INT priority);

        // Queries the current GPU thread priority.
        DIRECT3D9EX_API HRESULT getGpuThreadPriority(IDirect3DDevice9Ex *deviceEx, INT *outPriority);

        // Issues a WaitForVBlank call on the specified adapter.
        DIRECT3D9EX_API HRESULT waitForVBlank(IDirect3DDevice9Ex *deviceEx, UINT adapter);

        // Returns a short textual description for the most common device removal errors.
        DIRECT3D9EX_API char const *describeDeviceRemovedReason(HRESULT result);
}

#if defined(SWG_DIRECT3D9EX_SUPPORT_HEADER_ONLY)

namespace Direct3d9ExSupport
{
        namespace
        {
                inline bool isValidHandle(HMODULE module)
                {
                        return module != NULL && module != reinterpret_cast<HMODULE>(INVALID_HANDLE_VALUE);
                }

                inline PFN_Direct3DCreate9Ex resolveCreateProc(HMODULE module)
                {
                        return module ? reinterpret_cast<PFN_Direct3DCreate9Ex>(GetProcAddress(module, "Direct3DCreate9Ex")) : NULL;
                }
        }

        inline RuntimeHandles::RuntimeHandles()
        : module(NULL)
        , loaded(false)
        , createProc(NULL)
        {
        }

        inline HMODULE loadRuntime(bool loadIfMissing, bool *outLoaded)
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

        inline void unloadRuntime(HMODULE module, bool loaded)
        {
                if (loaded && isValidHandle(module))
                        FreeLibrary(module);
        }

        inline PFN_Direct3DCreate9Ex getCreate9ExProc(HMODULE module)
        {
                return resolveCreateProc(module);
        }

        inline HRESULT createInterface(PFN_Direct3DCreate9Ex createProc, UINT sdkVersion, IDirect3D9Ex **outInterface)
        {
                if (!outInterface)
                        return E_POINTER;

                *outInterface = NULL;
                if (!createProc)
                        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

                return createProc(sdkVersion, outInterface);
        }

        inline HRESULT createInterface(UINT sdkVersion, IDirect3D9Ex **outInterface)
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

        inline bool acquireRuntime(bool loadIfMissing, RuntimeHandles *outHandles)
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

        inline void releaseRuntime(RuntimeHandles &handles)
        {
                if (handles.module)
                {
                        unloadRuntime(handles.module, handles.loaded);
                        handles.module = NULL;
                }

                handles.loaded = false;
                handles.createProc = NULL;
        }

        inline HRESULT createInterface(const RuntimeHandles &handles, UINT sdkVersion, IDirect3D9Ex **outInterface)
        {
                return createInterface(handles.createProc, sdkVersion, outInterface);
        }

        inline bool isRuntimeAvailable()
        {
                HMODULE module = loadRuntime(false, NULL);
                if (!module)
                        return false;

                return resolveCreateProc(module) != NULL;
        }

        inline bool isDeviceRemovedError(HRESULT result)
        {
                return result == D3DERR_DEVICEREMOVED
                        || result == D3DERR_DEVICEHUNG
                        || result == D3DERR_DEVICELOST
                        || result == D3DERR_DRIVERINTERNALERROR;
        }

        inline UINT clampMaximumFrameLatency(UINT latency)
        {
                if (latency < 1u)
                        latency = 1u;
                if (latency > 16u)
                        latency = 16u;
                return latency;
        }

        inline INT clampGpuThreadPriority(INT priority)
        {
                if (priority < -7)
                        priority = -7;
                if (priority > 7)
                        priority = 7;
                return priority;
        }

        inline HRESULT setMaximumFrameLatency(IDirect3DDevice9Ex *deviceEx, UINT latency)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->SetMaximumFrameLatency(clampMaximumFrameLatency(latency));
        }

        inline HRESULT setGpuThreadPriority(IDirect3DDevice9Ex *deviceEx, INT priority)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->SetGPUThreadPriority(clampGpuThreadPriority(priority));
        }

        inline HRESULT getGpuThreadPriority(IDirect3DDevice9Ex *deviceEx, INT *outPriority)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->GetGPUThreadPriority(outPriority);
        }

        inline HRESULT waitForVBlank(IDirect3DDevice9Ex *deviceEx, UINT adapter)
        {
                if (!deviceEx)
                        return E_POINTER;
                return deviceEx->WaitForVBlank(adapter);
        }

        inline char const *describeDeviceRemovedReason(HRESULT result)
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

#endif // defined(SWG_DIRECT3D9EX_SUPPORT_HEADER_ONLY)

#endif // INCLUDED_Direct3d9ExSupport_H

