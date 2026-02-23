// ======================================================================
//
// DllExport_upgraded.cpp  (VS2013/D3D9-friendly)
// Based on the user's original DllExport.cpp
// - Removes inline asm (__asm int 3) that breaks x64 and /clr builds
// - Replaces with a portable DEBUG_BREAK() that uses __debugbreak() on MSVC
// - Logs stub invocations (OutputDebugStringA/stdio) and only breaks in _DEBUG builds
// - Keeps all method signatures and behavior (intentional "stub" no-op bodies)
// - Adds SAL-like unused parameter annotations to avoid warnings
// - Keeps Windows lean-and-mean includes and original project headers
//
// Notes for "maxing out DX9" in VS2013:
// * This file itself is a stub container; DX9 feature usage (D3D9Ex, SM3.0, FP16, sRGB) should be
//   implemented in the engine modules (e.g., device creation, swap chain, shaders). This file is
//   now safe for x86/x64 builds so the renderer can target the highest DX9 path.
// * Recommend: use IDirect3D9Ex when available (Vista+), D3DPRESENT_LINEAR_CONTENT in gamma-correct
//   workflows, and request highest available shader profiles (ps_3_0/vs_3_0).
//
// ======================================================================

#ifndef STRICT
#define STRICT 1
#endif

// Trim down windows.h
#define NOGDICAPMASKS
#define NOVIRTUALKEYCODE
#define NOKEYSTATES
#define NORASTEROPS
#define NOATOM
#define NOCOLOR
#define NODRAWTEXT
#define NOMEMMGR
#define NOMETAFILE
#define NOMINMAX
#define NOOPENFILE
#define NOSERVICE
#define NOSOUND
#define NOCOMM
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <wtypes.h>
#include <cstdio>

// Portable debug-break without inline asm (x86/x64 safe)
#ifndef DEBUG_BREAK
  #if defined(_MSC_VER)
    #include <intrin.h>
    #define DEBUG_BREAK() __debugbreak()
  #else
    #include <signal.h>
    #define DEBUG_BREAK() raise(SIGTRAP)
  #endif
#endif

namespace
{
        inline void dllExportReportStub(const char *functionName)
        {
#if defined(_MSC_VER)
                OutputDebugStringA("DllExport stub hit: ");
                OutputDebugStringA(functionName ? functionName : "<unknown>");
                OutputDebugStringA("\n");
#else
                std::fputs("DllExport stub hit: ", stderr);
                std::fputs(functionName ? functionName : "<unknown>", stderr);
                std::fputs("\n", stderr);
#endif
        }
}

#if defined(_DEBUG)
#define DLL_EXPORT_TRIGGER()              \
        do                                \
        {                                 \
                dllExportReportStub(__FUNCTION__); \
                DEBUG_BREAK();            \
        } while (false)
#else
#define DLL_EXPORT_TRIGGER()              \
        do                                \
        {                                 \
                dllExportReportStub(__FUNCTION__); \
        } while (false)
#endif

// Helper to mark unused parameters and avoid warnings
#ifndef UNUSED
  #define UNUSED(x) (void)(x)
#endif

// ======================================================================
// Original project headers (left intact so linkage stays the same)
// ======================================================================
#include "sharedFoundation/FirstSharedFoundation.h"

#include "clientGraphics/DynamicIndexBuffer.h"
#include "clientGraphics/DynamicVertexBuffer.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/Material.h"
#include "clientGraphics/ShaderImplementation.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureFormatInfo.h"
#include "clientGraphics/VertexBufferVector.h"
#include "sharedDebug/DataLint.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/DebugKey.h"
#include "sharedDebug/PerformanceTimer.h"
#include "sharedDebug/Profiler.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ConfigSharedFoundation.h"
#include "sharedFoundation/CrashReportInformation.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Fatal.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/PersistentCrcString.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedMath/Transform.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/Object.h"
#include "sharedSynchronization/Mutex.h"

// ======================================================================
// Minimal implementations (unchanged signatures).
// In Debug they break into the debugger; in Release they only log stub usage.
// ======================================================================

void Fatal(const char *fmt, ...)
{
	UNUSED(fmt);
	DLL_EXPORT_TRIGGER();
}

void DebugFatal(const char *fmt, ...)
{
	UNUSED(fmt);
	DLL_EXPORT_TRIGGER();
}

void Warning(const char *fmt, ...)
{
	UNUSED(fmt);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

void Report::setFlags(int flags)
{
	UNUSED(flags);
	DLL_EXPORT_TRIGGER();
}

void Report::vprintf(const char *fmt, va_list args)
{
	UNUSED(fmt); UNUSED(args);
	DLL_EXPORT_TRIGGER();
}

void Report::printf(const char *fmt, ...)
{
	UNUSED(fmt);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

bool ExitChain::isFataling()
{
	DLL_EXPORT_TRIGGER();
	return false;
}

// ======================================================================

bool ConfigSharedFoundation::getVerboseHardwareLogging()
{
	DLL_EXPORT_TRIGGER();
	return false;
}

// ======================================================================

Mutex::Mutex()
{
	DLL_EXPORT_TRIGGER();
}

Mutex::~Mutex()
{
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

const TextureFormatInfo &TextureFormatInfo::getInfo(TextureFormat f)
{
	UNUSED(f);
	DLL_EXPORT_TRIGGER();
	static TextureFormatInfo dummy;
	return dummy;
}

void TextureFormatInfo::setSupported(TextureFormat f, bool supported)
{
	UNUSED(f); UNUSED(supported);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

void *MemoryManager::allocate(size_t s, uint32 a, bool b1, bool b2)
{
	UNUSED(s); UNUSED(a); UNUSED(b1); UNUSED(b2);
	DLL_EXPORT_TRIGGER();
	return NULL;
}

void  MemoryManager::free(void *p, bool owned)
{
	UNUSED(p); UNUSED(owned);
	DLL_EXPORT_TRIGGER();
}

void  MemoryManager::own(void *p)
{
	UNUSED(p);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

bool DataLint::isEnabled()
{
	DLL_EXPORT_TRIGGER();
	return false;
}

// ======================================================================

void DebugFlags::registerFlag(bool &b, const char *n, const char *d)
{
	UNUSED(b); UNUSED(n); UNUSED(d);
	DLL_EXPORT_TRIGGER();
}

void DebugFlags::registerFlag(bool &b, const char *n, const char *d, ReportRoutine1 r, int pri)
{
	UNUSED(b); UNUSED(n); UNUSED(d); UNUSED(r); UNUSED(pri);
	DLL_EXPORT_TRIGGER();
}

void DebugFlags::unregisterFlag(bool &b)
{
	UNUSED(b);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

void DebugKey::registerFlag(bool &b, const char *name)
{
	UNUSED(b); UNUSED(name);
	DLL_EXPORT_TRIGGER();
}

// ----------------------------------------------------------------------

bool DebugKey::isPressed(int k)
{
	UNUSED(k);
	DLL_EXPORT_TRIGGER();
	return false;
}

// ----------------------------------------------------------------------

bool DebugKey::isDown(int k)
{
	UNUSED(k);
	DLL_EXPORT_TRIGGER();
	return false;
}

// ======================================================================

Material::Material()
{
	DLL_EXPORT_TRIGGER();
}

Material::~Material()
{
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

MemoryBlockManager::MemoryBlockManager(const char *n, bool t, int es, int ec, int ga, int al)
{
	UNUSED(n); UNUSED(t); UNUSED(es); UNUSED(ec); UNUSED(ga); UNUSED(al);
	DLL_EXPORT_TRIGGER();
}

MemoryBlockManager::~MemoryBlockManager()
{
	DLL_EXPORT_TRIGGER();
}

int MemoryBlockManager::getElementSize() const
{
	DLL_EXPORT_TRIGGER();
	return 0;
}

void *MemoryBlockManager::allocate(bool z)
{
	UNUSED(z);
	DLL_EXPORT_TRIGGER();
	return 0;
}

void MemoryBlockManager::free(void *p)
{
	UNUSED(p);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

bool Os::isMainThread(void)
{
	DLL_EXPORT_TRIGGER();
	return false;
}

Os::ThreadId Os::getThreadId()
{
	DLL_EXPORT_TRIGGER();
	return 0;
}

// ======================================================================

Transform const Transform::identity;

void Transform::multiply(const Transform &a, const Transform &b)
{
	UNUSED(a); UNUSED(b);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

const char *Shader::getName() const
{
	DLL_EXPORT_TRIGGER();
	return NULL;
}

bool StaticShader::getMaterial(Tag t, Material &m) const
{
	UNUSED(t); UNUSED(m);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::getTextureData(Tag t, StaticShaderTemplate::TextureData &td) const
{
	UNUSED(t); UNUSED(td);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::getTexture(Tag t, const Texture *&tex) const
{
	UNUSED(t); UNUSED(tex);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::getTextureCoordinateSet(Tag t, uint8 &set) const
{
	UNUSED(t); UNUSED(set);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::getTextureFactor(Tag t, uint32 &f) const
{
	UNUSED(t); UNUSED(f);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::getTextureScroll(Tag t, StaticShaderTemplate::TextureScroll &ts) const
{
	UNUSED(t); UNUSED(ts);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::getAlphaTestReferenceValue(Tag t, uint8 &v) const
{
	UNUSED(t); UNUSED(v);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::getStencilReferenceValue(Tag t, uint32 &v) const
{
	UNUSED(t); UNUSED(v);
	DLL_EXPORT_TRIGGER();
	return false;
}

bool StaticShader::containsPrecalculatedVertexLighting() const
{
	DLL_EXPORT_TRIGGER();
	return false;
}

// ======================================================================

void Texture::fetch() const
{
	DLL_EXPORT_TRIGGER();
}

void Texture::release() const
{
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

DynamicIndexBufferGraphicsData::~DynamicIndexBufferGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

DynamicVertexBufferGraphicsData::~DynamicVertexBufferGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

HardwareIndexBuffer::~HardwareIndexBuffer()
{
	DLL_EXPORT_TRIGGER();
}

StaticIndexBuffer::StaticIndexBuffer(int n)
: HardwareIndexBuffer(T_static)
{
	UNUSED(n);
	DLL_EXPORT_TRIGGER();
}

StaticIndexBuffer::~StaticIndexBuffer()
{
	DLL_EXPORT_TRIGGER();
}

StaticIndexBufferGraphicsData::~StaticIndexBufferGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

StaticShaderGraphicsData::~StaticShaderGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

ShaderImplementationGraphicsData::~ShaderImplementationGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

StaticVertexBufferGraphicsData::~StaticVertexBufferGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

TextureGraphicsData::~TextureGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

VertexBufferVectorGraphicsData::~VertexBufferVectorGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

ShaderImplementationPassVertexShaderGraphicsData::~ShaderImplementationPassVertexShaderGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

ShaderImplementationPassPixelShaderProgramGraphicsData::~ShaderImplementationPassPixelShaderProgramGraphicsData()
{
	DLL_EXPORT_TRIGGER();
}

char const * ShaderImplementationPassPixelShaderProgram::getFileName() const
{
	DLL_EXPORT_TRIGGER();
	return 0;
}

int ShaderImplementationPassPixelShaderProgram::getVersionMajor() const
{
	DLL_EXPORT_TRIGGER();
	return 0;
}

int ShaderImplementationPassPixelShaderProgram::getVersionMinor() const
{
	DLL_EXPORT_TRIGGER();
	return 0;
}

// ======================================================================

int ConfigFile::getKeyInt(const char *s, const char *k, int d, bool w)
{
	UNUSED(s); UNUSED(k); UNUSED(d); UNUSED(w);
	DLL_EXPORT_TRIGGER();
	return 0;
}

bool  ConfigFile::getKeyBool  (const char *s, const char *k, bool d, bool w)
{
	UNUSED(s); UNUSED(k); UNUSED(d); UNUSED(w);
	DLL_EXPORT_TRIGGER();
	return false;
}

// ======================================================================

real Clock::frameTime()
{
	DLL_EXPORT_TRIGGER();
	return 0.0f;
}

// ======================================================================

void Profiler::enter(char const *n)
{
	UNUSED(n);
	DLL_EXPORT_TRIGGER();
}

void Profiler::leave(char const *n)
{
	UNUSED(n);
	DLL_EXPORT_TRIGGER();
}

void Profiler::transfer(char const *from, char const *to)
{
	UNUSED(from); UNUSED(to);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

AbstractFile *TreeFile::open(const char *p, AbstractFile::PriorityType prio, bool b)
{
	UNUSED(p); UNUSED(prio); UNUSED(b);
	DLL_EXPORT_TRIGGER();
	return NULL;
}

// ======================================================================

CrcString::CrcString()
{
	DLL_EXPORT_TRIGGER();
}

CrcString::~CrcString()
{
	DLL_EXPORT_TRIGGER();
}

bool CrcString::operator < (CrcString const &rhs) const
{
	UNUSED(rhs);
	return false;
}

// ======================================================================

PersistentCrcString::PersistentCrcString(CrcString const &s)
{
	UNUSED(s);
	DLL_EXPORT_TRIGGER();
}

PersistentCrcString::~PersistentCrcString()
{
	DLL_EXPORT_TRIGGER();
}


char const * PersistentCrcString::getString() const
{
	DLL_EXPORT_TRIGGER();
	return NULL;
}

void PersistentCrcString::clear()
{
	DLL_EXPORT_TRIGGER();
}

void PersistentCrcString::set(char const *s, bool b)
{
	UNUSED(s); UNUSED(b);
	DLL_EXPORT_TRIGGER();
}

void PersistentCrcString::set(char const *s, uint32 c)
{
	UNUSED(s); UNUSED(c);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

TemporaryCrcString::TemporaryCrcString(char const *s, bool b)
{
	UNUSED(s); UNUSED(b);
	DLL_EXPORT_TRIGGER();
}

TemporaryCrcString::~TemporaryCrcString()
{
	DLL_EXPORT_TRIGGER();
}

char const * TemporaryCrcString::getString() const
{
	DLL_EXPORT_TRIGGER();
	return NULL;
}

void TemporaryCrcString::clear()
{
	DLL_EXPORT_TRIGGER();
}

void TemporaryCrcString::set(char const *s, bool b)
{
	UNUSED(s); UNUSED(b);
	DLL_EXPORT_TRIGGER();
}

void TemporaryCrcString::set(char const *s, uint32 c)
{
	UNUSED(s); UNUSED(c);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

void Graphics::setLastError(char const *c, char const *m)
{
	UNUSED(c); UNUSED(m);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

bool Graphics::writeImage(char const *path, int const w, int const h, int const pitch, int const *data, bool const topDown, Gl_imageFormat const fmt, Rectangle2d const *rect)
{
	UNUSED(path); UNUSED(w); UNUSED(h); UNUSED(pitch); UNUSED(data); UNUSED(topDown); UNUSED(fmt); UNUSED(rect);
	DLL_EXPORT_TRIGGER();
	return true;
}

// ======================================================================

void CrashReportInformation::addStaticText(char const *fmt, ...)
{
	UNUSED(fmt);
	DLL_EXPORT_TRIGGER();
}

// ======================================================================

PerformanceTimer::PerformanceTimer()
{
	DLL_EXPORT_TRIGGER();
}

PerformanceTimer::~PerformanceTimer()
{
	DLL_EXPORT_TRIGGER();
}

void PerformanceTimer::start()
{
	DLL_EXPORT_TRIGGER();
}

void PerformanceTimer::resume()
{
	DLL_EXPORT_TRIGGER();
}

void PerformanceTimer::stop()
{
	DLL_EXPORT_TRIGGER();
}

float PerformanceTimer::getElapsedTime() const
{
	DLL_EXPORT_TRIGGER();
	return 0.0f;
}

// ======================================================================

Transform const & Object::getTransform_o2w() const
{
	DLL_EXPORT_TRIGGER();
	return Transform::identity;
}

// ======================================================================

// Modern DllMain signature. Keep behavior identical (always TRUE).
BOOL APIENTRY DllMain(HINSTANCE hinstDll, DWORD fdwReason, LPVOID lpvReserved)
{
	UNUSED(hinstDll); UNUSED(fdwReason); UNUSED(lpvReserved);
	return TRUE;
}

// ======================================================================
