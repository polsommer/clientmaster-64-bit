// ============================================================================
//
// Direct3d10Bootstrap.cpp
//
// Runtime probing helpers that validate the availability of the Direct3D 10
// runtime components on Windows platforms.  The probes are deliberately
decoupled
// from the existing Direct3d9 implementation so that the migration effort can
// iterate safely without destabilising the shipping renderer.
//
// ============================================================================

#include "FirstDirect3d10.h"
#include "Direct3d10Bootstrap.h"

#ifdef _WIN32

#include <d3d10_1.h>
#include <d3d10.h>
#include <dxgi.h>

#include <cwchar>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace
{
        struct LibraryHandle
        {
                explicit LibraryHandle(HMODULE value = nullptr) : handle(value) {}
                ~LibraryHandle()
                {
                        if (handle)
                                FreeLibrary(handle);
                }

                LibraryHandle(const LibraryHandle &) = delete;
                LibraryHandle &operator=(const LibraryHandle &) = delete;

                LibraryHandle(LibraryHandle &&rhs) : handle(rhs.handle)
                {
                        rhs.handle = nullptr;
                }

                LibraryHandle &operator=(LibraryHandle &&rhs)
                {
                        if (this != &rhs)
                        {
                                if (handle)
                                        FreeLibrary(handle);
                                handle = rhs.handle;
                                rhs.handle = nullptr;
                        }
                        return *this;
                }

                FARPROC getProcAddress(const char *symbol) const
                {
                        if (!handle || !symbol)
                                return nullptr;
                        return GetProcAddress(handle, symbol);
                }

                HMODULE handle = nullptr;
        };

        template <typename T>
        struct ComPtr
        {
                ComPtr() = default;
                ~ComPtr()
                {
                        reset();
                }

                ComPtr(const ComPtr &) = delete;
                ComPtr &operator=(const ComPtr &) = delete;

                ComPtr(ComPtr &&rhs) : pointer(rhs.pointer)
                {
                        rhs.pointer = nullptr;
                }

                ComPtr &operator=(ComPtr &&rhs)
                {
                        if (this != &rhs)
                        {
                                reset();
                                pointer = rhs.pointer;
                                rhs.pointer = nullptr;
                        }
                        return *this;
                }

                void reset(T *value = nullptr)
                {
                        if (pointer)
                                pointer->Release();
                        pointer = value;
                }

                T **operator&()
                {
                        reset();
                        return &pointer;
                }

                T *get() const
                {
                        return pointer;
                }

                T *operator->() const
                {
                        return pointer;
                }

                explicit operator bool() const
                {
                        return pointer != nullptr;
                }

        private:
                T *pointer = nullptr;
        };

        std::string toUtf8(const wchar_t *source)
        {
                if (!source || !*source)
                        return std::string();

                const int length = static_cast<int>(std::wcslen(source));
                if (length <= 0)
                        return std::string();

                const int required = WideCharToMultiByte(CP_UTF8, 0, source, length, nullptr, 0, nullptr, nullptr);
                if (required <= 0)
                        return std::string();

                std::string buffer(required, '\0');
                WideCharToMultiByte(CP_UTF8, 0, source, length, &buffer[0], required, nullptr, nullptr);
                return buffer;
        }

        template <typename DeviceT>
        void validateCapabilities(DeviceT *device, Direct3d10Bootstrap::RuntimeProbe &probe)
        {
                if (!device)
                        return;

                struct FormatRequirement
                {
                        DXGI_FORMAT format;
                        UINT flags;
                        const char *label;
                };

                const FormatRequirement requirements[] = {
                        {DXGI_FORMAT_R8G8B8A8_UNORM,
                         D3D10_FORMAT_SUPPORT_RENDER_TARGET | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE,
                         "DXGI_FORMAT_R8G8B8A8_UNORM render/sample support"},
                        {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
                         D3D10_FORMAT_SUPPORT_RENDER_TARGET | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE,
                         "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB render/sample support"},
                        {DXGI_FORMAT_D24_UNORM_S8_UINT,
                         D3D10_FORMAT_SUPPORT_DEPTH_STENCIL,
                         "DXGI_FORMAT_D24_UNORM_S8_UINT depth-stencil support"},
                        {DXGI_FORMAT_BC3_UNORM,
                         D3D10_FORMAT_SUPPORT_SHADER_SAMPLE,
                         "DXGI_FORMAT_BC3_UNORM texture sampling"},
                };

                for (const FormatRequirement &requirement : requirements)
                {
                        UINT support = 0;
                        if (FAILED(device->CheckFormatSupport(requirement.format, &support))
                            || (support & requirement.flags) != requirement.flags)
                        {
                                probe.missingFeatures.push_back(requirement.label);
                        }
                }

                UINT qualityLevels = 0;
                if (FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &qualityLevels)) || qualityLevels == 0)
                {
                        probe.missingFeatures.push_back("4x MSAA for DXGI_FORMAT_R8G8B8A8_UNORM");
                }
                else if (qualityLevels < 4)
                {
                        probe.warnings.push_back("DX10 device exposes limited MSAA quality levels (4 samples)");
                }

                D3D10_FEATURE_DATA_THREADING threading = {};
                if (SUCCEEDED(device->CheckFeatureSupport(D3D10_FEATURE_THREADING, &threading, sizeof(threading))))
                {
                        if (!threading.DriverConcurrentCreates)
                                probe.missingFeatures.push_back("Driver concurrent resource creation");
                        if (!threading.DriverCommandLists)
                                probe.warnings.push_back("Driver does not support D3D10 command lists");
                }
        }

        LibraryHandle loadModule(const wchar_t *moduleName)
        {
                if (!moduleName)
                        return LibraryHandle();
                return LibraryHandle(LoadLibraryW(moduleName));
        }

        template <typename DeviceT>
        struct FeatureLevelExtractor;

        template <>
        struct FeatureLevelExtractor<ID3D10Device1>
        {
                static std::uint32_t extract(ID3D10Device1 *device)
                {
                        return device ? static_cast<std::uint32_t>(device->GetFeatureLevel()) : 0u;
                }
        };

        template <>
        struct FeatureLevelExtractor<ID3D10Device>
        {
                static std::uint32_t extract(ID3D10Device *)
                {
                        return static_cast<std::uint32_t>(D3D10_FEATURE_LEVEL_10_0);
                }
        };

        template <typename DeviceT>
        void finaliseDevice(DeviceT *device, Direct3d10Bootstrap::RuntimeProbe &probe)
        {
                if (!device)
                        return;

                probe.deviceCreated = true;
                probe.featureLevel = FeatureLevelExtractor<DeviceT>::extract(device);
                validateCapabilities(device, probe);
        }

        using CreateDXGIFactoryFn = HRESULT (WINAPI *)(REFIID, void **);
        using CreateDevice1Fn = HRESULT (WINAPI *)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, D3D10_FEATURE_LEVEL1, UINT, ID3D10Device1 **);
        using CreateDeviceFn = HRESULT (WINAPI *)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, ID3D10Device **);

        bool tryCreateHardwareDevice(IDXGIAdapter *adapter,
                CreateDevice1Fn createDevice1,
                CreateDeviceFn createDevice0,
                UINT deviceFlags,
                Direct3d10Bootstrap::RuntimeProbe &probe)
        {
                if (!adapter)
                        return false;

                if (createDevice1)
                {
                        const D3D10_FEATURE_LEVEL1 requestedLevels[] =
                        {
                                D3D10_FEATURE_LEVEL_10_1,
                                D3D10_FEATURE_LEVEL_10_0,
                                D3D10_FEATURE_LEVEL_9_3,
                                D3D10_FEATURE_LEVEL_9_2,
                                D3D10_FEATURE_LEVEL_9_1,
                        };

                        for (const auto level : requestedLevels)
                        {
                                ComPtr<ID3D10Device1> device1;
                                if (SUCCEEDED(createDevice1(adapter, D3D10_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, level, D3D10_1_SDK_VERSION, &device1)))
                                {
                                        finaliseDevice(device1.get(), probe);
                                        return true;
                                }
                        }
                }

                if (createDevice0)
                {
                        ComPtr<ID3D10Device> device0;
                        if (SUCCEEDED(createDevice0(adapter, D3D10_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, D3D10_SDK_VERSION, &device0)))
                        {
                                finaliseDevice(device0.get(), probe);
                                return true;
                        }
                }

                return false;
        }

        bool tryCreateWarpDevice(CreateDevice1Fn createDevice1,
                CreateDeviceFn createDevice0,
                UINT deviceFlags,
                Direct3d10Bootstrap::RuntimeProbe &probe)
        {
                if (createDevice1)
                {
                        ComPtr<ID3D10Device1> warpDevice;
                        if (SUCCEEDED(createDevice1(nullptr, D3D10_DRIVER_TYPE_WARP, nullptr, deviceFlags, D3D10_FEATURE_LEVEL_10_0, D3D10_1_SDK_VERSION, &warpDevice)))
                        {
                                finaliseDevice(warpDevice.get(), probe);
                                return true;
                        }
                }

                if (createDevice0)
                {
                        ComPtr<ID3D10Device> warpDevice;
                        if (SUCCEEDED(createDevice0(nullptr, D3D10_DRIVER_TYPE_WARP, nullptr, deviceFlags, D3D10_SDK_VERSION, &warpDevice)))
                        {
                                finaliseDevice(warpDevice.get(), probe);
                                return true;
                        }
                }

                return false;
        }

        bool isBetterAdapterCandidate(const Direct3d10Bootstrap::RuntimeProbe &candidate,
                const Direct3d10Bootstrap::RuntimeProbe &current)
        {
                if (!candidate.deviceCreated)
                        return false;
                if (!current.deviceCreated)
                        return true;

                const bool candidateReady = candidate.isReady();
                const bool currentReady = current.isReady();
                if (candidateReady != currentReady)
                        return candidateReady;

                if (candidate.featureLevel != current.featureLevel)
                        return candidate.featureLevel > current.featureLevel;

                if (candidate.dedicatedVideoMemory != current.dedicatedVideoMemory)
                        return candidate.dedicatedVideoMemory > current.dedicatedVideoMemory;

                return false;
        }
}

Direct3d10Bootstrap::RuntimeProbe Direct3d10Bootstrap::probe()
{
        RuntimeProbe probeResult;

        LibraryHandle dxgiModule = loadModule(L"dxgi.dll");
        if (dxgiModule.handle)
                probeResult.dxgiAvailable = true;
        else
                probeResult.missingDependencies.push_back("dxgi.dll");

        LibraryHandle d3d10Module = loadModule(L"d3d10.dll");
        if (d3d10Module.handle)
                probeResult.d3d10Available = true;

        LibraryHandle d3d10_1Module = loadModule(L"d3d10_1.dll");
        if (d3d10_1Module.handle)
                probeResult.d3d10_1Available = true;

        if (!probeResult.d3d10Available && !probeResult.d3d10_1Available)
        {
                probeResult.missingDependencies.push_back("d3d10.dll / d3d10_1.dll");
                return probeResult;
        }

        const CreateDXGIFactoryFn createFactory1 = reinterpret_cast<CreateDXGIFactoryFn>(dxgiModule.getProcAddress("CreateDXGIFactory1"));
        const CreateDXGIFactoryFn createFactory = reinterpret_cast<CreateDXGIFactoryFn>(dxgiModule.getProcAddress("CreateDXGIFactory"));

        ComPtr<IDXGIFactory1> factory1;
        ComPtr<IDXGIFactory> factory0;
        if (createFactory1)
        {
                if (FAILED(createFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory1))))
                        factory1.reset();
        }

        if (!factory1 && createFactory)
        {
                        if (FAILED(createFactory(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory0))))
                                factory0.reset();
        }

        IDXGIFactory *factory = factory1 ? static_cast<IDXGIFactory *>(factory1.get()) : factory0.get();
        if (!factory)
        {
                probeResult.missingDependencies.push_back("Unable to create a DXGI factory");
                return probeResult;
        }

        auto resolveDevice1 = [&]() -> CreateDevice1Fn
        {
                CreateDevice1Fn fn = nullptr;
                if (d3d10_1Module.handle)
                        fn = reinterpret_cast<CreateDevice1Fn>(d3d10_1Module.getProcAddress("D3D10CreateDevice1"));
                if (!fn && d3d10Module.handle)
                        fn = reinterpret_cast<CreateDevice1Fn>(d3d10Module.getProcAddress("D3D10CreateDevice1"));
                return fn;
        };

        const CreateDevice1Fn createDevice1 = resolveDevice1();
        const CreateDeviceFn createDevice0 = d3d10Module.handle
                ? reinterpret_cast<CreateDeviceFn>(d3d10Module.getProcAddress("D3D10CreateDevice"))
                : nullptr;

        UINT deviceFlags = D3D10_CREATE_DEVICE_BGRA_SUPPORT;

        RuntimeProbe bestProbe = probeResult;
        bool haveBestProbe = false;
        std::vector<std::string> adapterDiagnostics;
        bool enumeratedAdapters = false;

        for (UINT adapterIndex = 0;; ++adapterIndex)
        {
                ComPtr<IDXGIAdapter> adapter;
                const HRESULT enumResult = factory->EnumAdapters(adapterIndex, &adapter);
                if (enumResult == DXGI_ERROR_NOT_FOUND)
                        break;
                if (FAILED(enumResult))
                        continue;

                enumeratedAdapters = true;

                DXGI_ADAPTER_DESC adapterDesc = {};
                if (FAILED(adapter->GetDesc(&adapterDesc)))
                        continue;

                if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
                {
                        adapterDiagnostics.emplace_back("Skipping software adapter '" + toUtf8(adapterDesc.Description) + "'.");
                        continue;
                }

                RuntimeProbe candidate = probeResult;
                candidate.adapterDescription = toUtf8(adapterDesc.Description);
                candidate.dedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
                candidate.vendorId = static_cast<std::uint32_t>(adapterDesc.VendorId);
                candidate.deviceId = static_cast<std::uint32_t>(adapterDesc.DeviceId);
                candidate.isAmdAdapter = (adapterDesc.VendorId == 0x1002u) || (adapterDesc.VendorId == 0x1022u);
                candidate.isNvidiaAdapter = (adapterDesc.VendorId == 0x10deu);
                candidate.isIntelAdapter = (adapterDesc.VendorId == 0x8086u);

                if (!tryCreateHardwareDevice(adapter.get(), createDevice1, createDevice0, deviceFlags, candidate))
                {
                        std::string diagnostic("Adapter '");
                        diagnostic += candidate.adapterDescription.empty() ? std::string("unknown") : candidate.adapterDescription;
                        diagnostic += "' failed Direct3D 10 device creation.";
                        adapterDiagnostics.push_back(diagnostic);
                        continue;
                }

                if (!haveBestProbe || isBetterAdapterCandidate(candidate, bestProbe))
                {
                        bestProbe = candidate;
                        haveBestProbe = true;
                }
        }

        if (haveBestProbe)
        {
                        if (!adapterDiagnostics.empty())
                                bestProbe.warnings.insert(bestProbe.warnings.end(), adapterDiagnostics.begin(), adapterDiagnostics.end());
                        return bestProbe;
        }

        RuntimeProbe warpProbe = probeResult;
        if (tryCreateWarpDevice(createDevice1, createDevice0, deviceFlags, warpProbe))
        {
                warpProbe.adapterDescription = "Microsoft WARP adapter";
                warpProbe.vendorId = 0x1414u;
                warpProbe.deviceId = 0x8cu;
                warpProbe.warnings.push_back("Hardware D3D10 device unavailable, using WARP fallback");
                if (!adapterDiagnostics.empty())
                        warpProbe.warnings.insert(warpProbe.warnings.end(), adapterDiagnostics.begin(), adapterDiagnostics.end());
                return warpProbe;
        }

        if (!enumeratedAdapters)
                probeResult.missingDependencies.push_back("No graphics adapters expose Direct3D 10 support");
        else
                probeResult.missingFeatures.push_back("Failed to create a Direct3D 10 device");

        if (!adapterDiagnostics.empty())
                probeResult.warnings.insert(probeResult.warnings.end(), adapterDiagnostics.begin(), adapterDiagnostics.end());

        if (probeResult.adapterDescription.empty())
                probeResult.adapterDescription = "Unknown adapter";

        return probeResult;
}

#endif // _WIN32
