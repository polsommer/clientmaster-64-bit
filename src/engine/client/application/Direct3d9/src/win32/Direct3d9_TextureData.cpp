// ======================================================================
// Direct3d9_TextureData.cpp
//
// Portions Copyright 1998 Bootprint Entertainment
// Portions copyright 2000-2002 Sony Online Entertainment
// All Rights Reserved.
// ======================================================================

#include "FirstDirect3d9.h"
#include "Direct3d9_TextureData.h"

#include "Direct3d9.h"
#include "Direct3d9_Metrics.h"
#include "Direct3d9_StateCache.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedFoundation/Os.h"
#include "clientGraphics/TextureFormatInfo.h"

#include <d3dx9tex.h>
#include <map>

// ======================================================================

const Tag TAG_ENVM = TAG(E,N,V,M);

// ======================================================================

Direct3d9_TextureData::PixelFormatInfo     Direct3d9_TextureData::ms_pixelFormatInfoArray[TF_Count];
MemoryBlockManager                        *Direct3d9_TextureData::ms_memoryBlockManager;
Direct3d9_TextureData::GlobalTextureList  *Direct3d9_TextureData::ms_globalTextureList;

static D3DFORMAT translationTable[] =
{
        D3DFMT_A8R8G8B8,      // TF_ARGB_8888,
        D3DFMT_A4R4G4B4,      // TF_ARGB_4444,
        D3DFMT_A1R5G5B5,      // TF_ARGB_1555,
        D3DFMT_X8R8G8B8,      // TF_XRGB_8888,
        D3DFMT_R8G8B8,        // TF_RGB_888,
        D3DFMT_R5G6B5,        // TF_RGB_565,
        D3DFMT_X1R5G5B5,      // TF_RGB_555,
        D3DFMT_DXT1,          // TF_DXT1,
        D3DFMT_DXT2,          // TF_DXT2,
        D3DFMT_DXT3,          // TF_DXT3,
        D3DFMT_DXT4,          // TF_DXT4,
        D3DFMT_DXT5,          // TF_DXT5,
        D3DFMT_A8,            // TF_A_8,
        D3DFMT_L8,            // TF_L_8
        D3DFMT_P8,            // TF_P_8,
        D3DFMT_A16B16G16R16F, // TF_ABGR_16F
        D3DFMT_A32B32G32R32F, // TF_ABGR_32F
        D3DFMT_UNKNOWN,       // TF_Count
        D3DFMT_UNKNOWN        // TF_Native
};

// ----------------------------------------------------------------------

static char const *getTextureName(Texture const &texture)
{
	char const * const name = texture.getName();
	return (name && *name) ? name : "<unnamed>";
}

// ----------------------------------------------------------------------

static char const *getResourceTypeName(D3DRESOURCETYPE const resourceType)
{
	switch (resourceType)
	{
		case D3DRTYPE_TEXTURE:
			return "2D";

		case D3DRTYPE_CUBETEXTURE:
			return "Cube";

		case D3DRTYPE_VOLUMETEXTURE:
			return "Volume";

		default:
			return "Unknown";
	}
}

// ----------------------------------------------------------------------

static bool checkTextureFormat(IDirect3D9 &direct3d, D3DFORMAT format)
{
        const HRESULT hresult = direct3d.CheckDeviceFormat(Direct3d9::getAdapter(), Direct3d9::getDeviceType(), Direct3d9::getAdapterFormat(), 0, D3DRTYPE_TEXTURE, format);
        if (hresult == D3D_OK)
                return true;
        if (hresult == D3DERR_NOTAVAILABLE)
                return false;

        FATAL_DX_HR("CheckDeviceFormat failed %s", hresult);
        return false;
}

// ======================================================================
/**
 * install the Direct3d texture data support.
 *
 * This method performs essential initialization for this api's texture
 * system.  It will map Gl-defined texture formats to physical
 * texture formats available for the selected device in addition to
 * performing other required initialization.
 */

void Direct3d9_TextureData::install(void)
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Direct3d9_TextureData already installed"));
	ms_memoryBlockManager = new MemoryBlockManager("Direct3d9_TextureData::memoryBlockManager", true, sizeof(Direct3d9_TextureData), 0, 0, 0);

	IDirect3D9 *direct3d = Direct3d9::getDirect3d();
        for (int i = 0; i < TF_Count; ++i)
        {
                bool supported = false;
                D3DFORMAT native = translationTable[i];
                if (native != D3DFMT_UNKNOWN)
                {
                        supported = checkTextureFormat(*direct3d, native);
                        if (!supported && i == TF_RGB_555)
                        {
                                // Prefer X1R5G5B5 for TF_RGB_555 but fall back to a compatible 1-bit alpha variant if unavailable.
                                const D3DFORMAT fallback = D3DFMT_A1R5G5B5;
                                if (fallback != native && checkTextureFormat(*direct3d, fallback))
                                {
                                        native = fallback;
                                        translationTable[i] = fallback;
                                        supported = true;
                                }
                        }
                }

                TextureFormatInfo::setSupported(static_cast<TextureFormat>(i), supported);
        }

	ms_globalTextureList = new GlobalTextureList;
}

// ----------------------------------------------------------------------
/**
 * Remove the Direct3d 6 texture data support.
 */

void Direct3d9_TextureData::remove(void)
{
	if (!ExitChain::isFataling())
	{
		if (ms_memoryBlockManager)
		{
			DEBUG_FATAL(!ms_memoryBlockManager, ("Direct3d9_TextureData not installed"));
			delete ms_memoryBlockManager;
			ms_memoryBlockManager = 0;
		}

		if (ms_globalTextureList)
		{
			// free all the textures
			while (!ms_globalTextureList->empty())
			{
				ms_globalTextureList->begin()->second.engineTexture->release();
				ms_globalTextureList->erase(ms_globalTextureList->begin());
			}

			DEBUG_FATAL(!ms_globalTextureList->empty(), ("global textures are still allocated in the graphics dll"));
			delete ms_globalTextureList;
			ms_globalTextureList = 0;
		}
	}
}

// ----------------------------------------------------------------------

void Direct3d9_TextureData::releaseAllGlobalTextures(void)
{
	while (!ms_globalTextureList->empty())
	{
		ms_globalTextureList->begin()->second.engineTexture->release();
		ms_globalTextureList->erase(ms_globalTextureList->begin());
	}
}

// ----------------------------------------------------------------------

void *Direct3d9_TextureData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof (Direct3d9_TextureData), ("bad size"));
	DEBUG_FATAL(size != static_cast<size_t> (ms_memoryBlockManager->getElementSize()), ("installed with bad size"));

	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d9_TextureData::operator delete(void *pointer)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(pointer);
}

// ----------------------------------------------------------------------

Direct3d9_TextureData const * const *Direct3d9_TextureData::getGlobalTexture(Tag tag)
{
	DEBUG_FATAL(!ms_globalTextureList, ("Not installed"));
	DEBUG_FATAL(!isGlobalTexture(tag), ("Tag is not correct for a global texture"));
	GlobalTextureList::const_iterator i = ms_globalTextureList->find(tag);
	DEBUG_FATAL(i == ms_globalTextureList->end(), ("Could not find requested global texture"));
	return & i->second.d3dTexture;
}

// ----------------------------------------------------------------------

bool Direct3d9_TextureData::isGlobalTexture(Tag tag)
{
	return (tag == TAG_ENVM) || ((tag >> 24) & 0xff) == '_';
}

// ----------------------------------------------------------------------

void Direct3d9_TextureData::setGlobalTexture(Tag tag, const Texture &texture)
{
	DEBUG_FATAL(!isGlobalTexture(tag), ("Tag is not correct for a global texture"));

	GlobalTextureList::iterator i = ms_globalTextureList->find(tag);
	if (i != ms_globalTextureList->end())
	{
		if (i->second.engineTexture != &texture)
		{
			texture.fetch();
			i->second.engineTexture->release();
			i->second.engineTexture = &texture;
			i->second.d3dTexture = static_cast<Direct3d9_TextureData const *>(texture.getGraphicsData());
		}
	}
	else
	{
		GlobalTextureInfo gti;
		gti.engineTexture = &texture;
		gti.d3dTexture = static_cast<Direct3d9_TextureData const *>(texture.getGraphicsData());
		const bool inserted = ms_globalTextureList->insert(GlobalTextureList::value_type(tag, gti)).second;
		UNREF(inserted);
		DEBUG_FATAL(!inserted, ("item was already present in map"));
		texture.fetch();
	}
}

// ----------------------------------------------------------------------

D3DFORMAT Direct3d9_TextureData::getD3dFormat(TextureFormat textureFormat)
{
	return translationTable[textureFormat];
}

// ----------------------------------------------------------------------

#ifdef _DEBUG

static int calculateTextureMemorySize(TextureFormat format, int width, int height, int depth, int mipMaps)
{
	int result = 0;

	TextureFormatInfo const & textureInfo = TextureFormatInfo::getInfo(format);
	for (int i = 0; i < mipMaps; ++i)
	{
		int w = width >> i;
		int h = height >> i;
		int d = depth >> i;
		if (w == 0)
			w = 1;
		if (h == 0)
			h = 1;
		if (d == 0)
			d = 1;

		if (textureInfo.compressed)
		{
			w /= textureInfo.blockWidth;
			h  /= textureInfo.blockHeight;
			if (w == 0)
				w = 1;
			if (h == 0)
				h = 1;
			result += textureInfo.blockSize * w * h * d;
		}
		else
		{
			result += textureInfo.pixelByteCount * w * h * d;
		}
	}

	return result;
}

#endif

// ======================================================================

Direct3d9_TextureData::Direct3d9_TextureData(const Texture &newEngineTexture, const TextureFormat *runtimeFormats, int numberOfRuntimeFormats)
:	TextureGraphicsData(),
	m_engineTexture(newEngineTexture),
	m_d3dTexture(NULL),
	m_destFormat(TF_RGB_555)
#ifdef _DEBUG
	,
	m_lastUsedFrameNumber(-1),
	m_memorySize(0)
#endif
{
	DEBUG_FATAL(!Os::isMainThread(), ("Creating texture from alternate thread"));

	DWORD   resourceUsage = 0;
	D3DPOOL resourcePool = D3DPOOL_MANAGED;
	{
		const bool isDynamic      = newEngineTexture.isDynamic();
		const bool isRenderTarget = newEngineTexture.isRenderTarget();
		if (isRenderTarget)
		{
			DEBUG_FATAL(newEngineTexture.getWidth() > Direct3d9::getMaxRenderTargetWidth() || newEngineTexture.getHeight() > Direct3d9::getMaxRenderTargetHeight(), ("Cannot create a render target larger than the primary surface (%d,%d) vs (%d,%d)", newEngineTexture.getWidth(), newEngineTexture.getHeight(), Direct3d9::getMaxRenderTargetWidth(), Direct3d9::getMaxRenderTargetHeight()));
			resourceUsage = D3DUSAGE_RENDERTARGET;
			resourcePool = D3DPOOL_DEFAULT;
		}
		else if (isDynamic)
		{
			resourceUsage = D3DUSAGE_DYNAMIC;
			resourcePool = D3DPOOL_DEFAULT;
		}
	}

	D3DRESOURCETYPE resourceType = D3DRTYPE_TEXTURE;
	const bool isCriticalTexture = newEngineTexture.isRenderTarget() || newEngineTexture.isDynamic();
	
	if (newEngineTexture.isCubeMap())
		resourceType = D3DRTYPE_CUBETEXTURE;
	else if (newEngineTexture.isVolumeMap())
		resourceType = D3DRTYPE_VOLUMETEXTURE;

	for (int i = 0; !m_d3dTexture && i < numberOfRuntimeFormats; ++i)
	{
		TextureFormatInfo const & textureInfo = TextureFormatInfo::getInfo(runtimeFormats[i]);
		if (textureInfo.supported)
		{
			const int textureWidth = m_engineTexture.getWidth() > 0 ? m_engineTexture.getWidth() : 1;
			const int textureHeight = m_engineTexture.getHeight() > 0 ? m_engineTexture.getHeight() : 1;
			const int textureDepth = m_engineTexture.getDepth() > 0 ? m_engineTexture.getDepth() : 1;
			const int mipmapLevelCount = m_engineTexture.getMipmapLevelCount() > 0 ? m_engineTexture.getMipmapLevelCount() : 1;

			if (textureWidth != m_engineTexture.getWidth() || textureHeight != m_engineTexture.getHeight() || textureDepth != m_engineTexture.getDepth() || mipmapLevelCount != m_engineTexture.getMipmapLevelCount())
				WARNING(true, ("Texture created with invalid dimensions/mipmap count (%d,%d,%d,%d); using (%d,%d,%d,%d) to avoid D3DERR_INVALIDCALL.\n", m_engineTexture.getWidth(), m_engineTexture.getHeight(), m_engineTexture.getDepth(), m_engineTexture.getMipmapLevelCount(), textureWidth, textureHeight, textureDepth, mipmapLevelCount));

			m_destFormat = runtimeFormats[i];
			D3DFORMAT d3dFormat = translationTable[m_destFormat];
			char const * const textureName = getTextureName(newEngineTexture);

			switch (resourceType)
			{
				case D3DRTYPE_TEXTURE:
					{
						IDirect3DTexture9 *texture = 0;
						const HRESULT hresult = Direct3d9::getDevice()->CreateTexture(textureWidth, textureHeight, mipmapLevelCount, resourceUsage, translationTable[m_destFormat], resourcePool, &texture, NULL);
						if (hresult == D3D_OK && texture)
							m_d3dTexture = texture;
						else
							WARNING(true, ("CreateTexture failed %s [name=%s type=%s dims=%dx%dx%d mip=%d textureFormat=%s d3dFormat=0x%08x usage=0x%08x pool=%d]", hresult, textureName, getResourceTypeName(resourceType), textureWidth, textureHeight, textureDepth, mipmapLevelCount, textureInfo.name, static_cast<unsigned int>(d3dFormat), static_cast<unsigned int>(resourceUsage), static_cast<int>(resourcePool)));

#ifdef _DEBUG
						m_memorySize = calculateTextureMemorySize(m_destFormat, m_engineTexture.getWidth(), m_engineTexture.getHeight(), m_engineTexture.getDepth(), m_engineTexture.getMipmapLevelCount());
#endif
					}
					break;

				case D3DRTYPE_CUBETEXTURE:
					{
						// create the texture surface
						DEBUG_FATAL(textureWidth != textureHeight, ("Cube textures must be square"));
						IDirect3DCubeTexture9 *texture = 0;
						HRESULT hresult = Direct3d9::getDevice()->CreateCubeTexture(textureWidth, mipmapLevelCount, resourceUsage, d3dFormat, resourcePool, &texture, NULL);
						if (hresult == D3D_OK && texture)
							m_d3dTexture = texture;
						else
							WARNING(true, ("CreateCubeTexture failed %s [name=%s type=%s dims=%dx%dx%d mip=%d textureFormat=%s d3dFormat=0x%08x usage=0x%08x pool=%d]", hresult, textureName, getResourceTypeName(resourceType), textureWidth, textureHeight, textureDepth, mipmapLevelCount, textureInfo.name, static_cast<unsigned int>(d3dFormat), static_cast<unsigned int>(resourceUsage), static_cast<int>(resourcePool)));

#ifdef _DEBUG
						m_memorySize = 6 * calculateTextureMemorySize(m_destFormat, m_engineTexture.getWidth(), m_engineTexture.getHeight(), m_engineTexture.getDepth(), m_engineTexture.getMipmapLevelCount());
#endif
					}
					break;

				case D3DRTYPE_VOLUMETEXTURE:
					{
						IDirect3DVolumeTexture9 *texture = 0;
						const HRESULT hresult = Direct3d9::getDevice()->CreateVolumeTexture(textureWidth, 
							textureHeight, 
							textureDepth, 
							mipmapLevelCount, 
							resourceUsage, 
							translationTable[m_destFormat], 
							resourcePool, &texture, NULL);
						if (hresult == D3D_OK && texture)
							m_d3dTexture = texture;
						else
							WARNING(true, ("CreateVolumeTexture failed %s [name=%s type=%s dims=%dx%dx%d mip=%d textureFormat=%s d3dFormat=0x%08x usage=0x%08x pool=%d]", hresult, textureName, getResourceTypeName(resourceType), textureWidth, textureHeight, textureDepth, mipmapLevelCount, textureInfo.name, static_cast<unsigned int>(d3dFormat), static_cast<unsigned int>(resourceUsage), static_cast<int>(resourcePool)));

#ifdef _DEBUG
						m_memorySize = calculateTextureMemorySize(m_destFormat, m_engineTexture.getWidth(), m_engineTexture.getHeight(), m_engineTexture.getDepth(), m_engineTexture.getMipmapLevelCount());
#endif
					}
					break;

				default:
					DEBUG_FATAL(true, ("unknown texture type"));
					break;
			}
		}
	}

	if (!m_d3dTexture)
	{
		char buffer[1024] = "";
		for (int i = 0; i < numberOfRuntimeFormats; ++i)
		{
			strcat(buffer, TextureFormatInfo::getInfo(runtimeFormats[i]).name);
			strcat(buffer, " ");
		}

		if (isCriticalTexture)
			DEBUG_FATAL(true, ("failed to create critical texture [name=%s type=%s dims=%dx%dx%d mip=%d usage=0x%08x pool=%d formats=%s]", getTextureName(newEngineTexture), getResourceTypeName(resourceType), m_engineTexture.getWidth(), m_engineTexture.getHeight(), m_engineTexture.getDepth(), m_engineTexture.getMipmapLevelCount(), static_cast<unsigned int>(resourceUsage), static_cast<int>(resourcePool), buffer));
		else
			WARNING(true, ("failed to create content texture [name=%s type=%s dims=%dx%dx%d mip=%d usage=0x%08x pool=%d formats=%s] (caller should substitute default texture)", getTextureName(newEngineTexture), getResourceTypeName(resourceType), m_engineTexture.getWidth(), m_engineTexture.getHeight(), m_engineTexture.getDepth(), m_engineTexture.getMipmapLevelCount(), static_cast<unsigned int>(resourceUsage), static_cast<int>(resourcePool), buffer));
	}

#ifdef _DEBUG
	Direct3d9_Metrics::textureMemoryTotal += m_memorySize;
	Direct3d9_Metrics::textureMemoryCreated += m_memorySize;
#endif
}

// ----------------------------------------------------------------------

Direct3d9_TextureData::~Direct3d9_TextureData(void)
{
#ifdef _DEBUG
	Direct3d9_Metrics::textureMemoryTotal -= m_memorySize;
	Direct3d9_Metrics::textureMemoryDestroyed += m_memorySize;
#endif

	if (m_d3dTexture)
	{
		Direct3d9_StateCache::destroyTexture(this);
		IGNORE_RETURN(m_d3dTexture->Release());
		m_d3dTexture = NULL;
	}
}

// ----------------------------------------------------------------------

TextureFormat Direct3d9_TextureData::getNativeFormat() const
{
	return m_destFormat;
}

// ----------------------------------------------------------------------

void Direct3d9_TextureData::lock(LockData &lockData)
{
	DEBUG_FATAL(lockData.getWidth() == 0, ("Locking 0 width area"));
	DEBUG_FATAL(lockData.getHeight() == 0, ("Locking 0 height area"));
	DEBUG_FATAL(lockData.getDepth() == 0, ("Locking 0 depth area"));

	DWORD flags = 0;
	if (lockData.isReadOnly()) 
	{
		flags |= D3DLOCK_READONLY;
	}

	if (lockData.getFormat() == TF_Native)
		lockData.m_format = m_destFormat;

	if (!m_d3dTexture)
	{
		TextureFormatInfo const &textureInfo = TextureFormatInfo::getInfo(lockData.getFormat());

		int const width = lockData.getWidth() > 0 ? lockData.getWidth() : 1;
		int const height = lockData.getHeight() > 0 ? lockData.getHeight() : 1;
		int const depth = lockData.getDepth() > 0 ? lockData.getDepth() : 1;

		int pitch = 0;
		int slicePitch = 0;
		if (textureInfo.compressed)
		{
			int const blockWidth = textureInfo.blockWidth > 0 ? textureInfo.blockWidth : 1;
			int const blockHeight = textureInfo.blockHeight > 0 ? textureInfo.blockHeight : 1;
			int const blocksWide = (width + blockWidth - 1) / blockWidth;
			int const blocksHigh = (height + blockHeight - 1) / blockHeight;
			pitch = blocksWide * textureInfo.blockSize;
			slicePitch = pitch * blocksHigh;
		}
		else
		{
			pitch = width * textureInfo.pixelByteCount;
			slicePitch = pitch * height;
		}

		int const bufferSize = slicePitch * depth;
		lockData.m_reserved = new uint8[static_cast<size_t>(bufferSize)];
		memset(lockData.m_reserved, 0, static_cast<size_t>(bufferSize));
		lockData.m_pixelData = lockData.m_reserved;
		lockData.m_pitch = pitch;
		lockData.m_slicePitch = slicePitch;

		WARNING(true, ("Locking fallback staging memory because texture allocation previously failed [name=%s level=%d dims=%dx%dx%d format=%s]", getTextureName(m_engineTexture), lockData.getLevel(), width, height, depth, textureInfo.name));
		return;
	}

	HRESULT hresult;

	bool const directNativeLock = (lockData.getFormat() == m_destFormat) && !m_engineTexture.isRenderTarget();

	// handle locking in native format when the resource can be locked directly
	if (directNativeLock)
	{
		if (m_engineTexture.isVolumeMap())
		{
			D3DLOCKED_BOX lockedBox;

			D3DBOX box;
			box.Left   = lockData.getX();
			box.Top    = lockData.getY();
			box.Right  = lockData.getX() + lockData.getWidth();
			box.Bottom = lockData.getY() + lockData.getHeight();
			box.Front  = lockData.getZ();
			box.Back   = lockData.getZ() + lockData.getDepth();

			const bool wholeTexture = (
				   box.Left   ==0
				&& box.Top    ==0
				&& box.Front  ==0
				&& box.Right  ==unsigned(m_engineTexture.getWidth())
				&& box.Bottom ==unsigned(m_engineTexture.getHeight())
				&& box.Back   ==unsigned(m_engineTexture.getDepth())
			);

			D3DBOX *pBox = (wholeTexture) ? (D3DBOX *)0 : &box;

			hresult = static_cast<IDirect3DVolumeTexture9*>(m_d3dTexture)->LockBox(lockData.getLevel(), &lockedBox, pBox, flags);
			FATAL_DX_HR("LockBox failed %s", hresult);

			// let the user know where and how to write
			lockData.m_pixelData = lockedBox.pBits;
			lockData.m_pitch = lockedBox.RowPitch;
			lockData.m_slicePitch = lockedBox.SlicePitch;
		}
		else
		{
			D3DLOCKED_RECT lockedRect;

			if (lockData.shouldDiscardContents())
			{
				flags |= D3DLOCK_DISCARD;
			}

			RECT r, *pr=&r;
			r.left   = lockData.getX();
			r.top    = lockData.getY();
			r.right  = lockData.getX() + lockData.getWidth();
			r.bottom = lockData.getY() + lockData.getHeight();

			if (  r.left==0
				&& r.top==0
				&& r.right==m_engineTexture.getWidth()
				&& r.bottom==m_engineTexture.getHeight()
				)
			{
				pr=0;
			}

			if (m_engineTexture.isCubeMap())
			{
				hresult = static_cast<IDirect3DCubeTexture9*>(m_d3dTexture)->LockRect(Direct3d9::getD3dCubeFace(lockData.m_cubeFace), lockData.getLevel(), &lockedRect, pr, flags);
				FATAL_DX_HR("LockRect failed %s", hresult);
			}
			else
			{
				hresult = static_cast<IDirect3DTexture9*>(m_d3dTexture)->LockRect(lockData.getLevel(), &lockedRect, pr, flags);
				FATAL_DX_HR("LockRect failed %s", hresult);
			}

			// let the user know where and how to write
			lockData.m_pixelData = lockedRect.pBits;
			lockData.m_pitch = lockedRect.Pitch;
			lockData.m_slicePitch = lockedRect.Pitch * lockData.getHeight();
		}

		lockData.m_reserved = NULL;
	}
	else
	{
		FATAL(m_engineTexture.isVolumeMap(), ("Volume map not supported for format conversion (yet)"));

		// the user wants to access the data in a non-native format.  This will not be cheap.

		// DXT textures have a minimum width and height of 4
		int width = lockData.getWidth();
		int height = lockData.getHeight();
		if (lockData.getFormat() == TF_DXT1 || lockData.getFormat() == TF_DXT2 || lockData.getFormat() == TF_DXT3 || lockData.getFormat() == TF_DXT4 || lockData.getFormat() == TF_DXT5)
		{
			if (width < 4)
				width = 4;
			if (height < 4)
				height = 4;
		}

		// create a temporary surface of the format desired by the user
		IDirect3DSurface9 * plainSurface = 0;
		hresult = Direct3d9::getDevice()->CreateOffscreenPlainSurface(width, height, translationTable[lockData.getFormat()], D3DPOOL_SCRATCH, &plainSurface, NULL);
		FATAL_DX_HR("CreateOffscreenPlainSurface failed %s", hresult);
		NOT_NULL(plainSurface);

		// record the temporary surface into the lock data
		lockData.m_reserved = plainSurface;

		// copy the original pixel bits back to the temporary surface if the contents aren't being discarded
		if (!lockData.shouldDiscardContents())
		{
			// get the d3d surface containing the texture bits
			IDirect3DSurface9 * surface = 0;
			if (m_engineTexture.isCubeMap())
			{
				hresult = static_cast<IDirect3DCubeTexture9*>(m_d3dTexture)->GetCubeMapSurface(Direct3d9::getD3dCubeFace(lockData.m_cubeFace), lockData.getLevel(), &surface);
				FATAL_DX_HR("GetCubeMapSurface failed %s", hresult);
			}
			else
			{
				hresult = static_cast<IDirect3DTexture9*>(m_d3dTexture)->GetSurfaceLevel(lockData.getLevel(), &surface);
				FATAL_DX_HR("GetSurfaceLevel failed %s", hresult);
			}

			// we only want to copy the locked data back
			RECT source;
			source.left   = lockData.getX();
			source.top    = lockData.getY();
			source.right  = lockData.getX() + lockData.getWidth();
			source.bottom = lockData.getY() + lockData.getHeight();

			// copy and convert the texture bits
			hresult = D3DXLoadSurfaceFromSurface(plainSurface, NULL, NULL, surface, NULL, &source, D3DX_FILTER_NONE, 0);
			FATAL_DX_HR("D3DXLoadSurfaceFromSurface failed %s", hresult);

			// release the d3d surface
			surface->Release();
		}

		// lock the temporary surface for the user to write to
		D3DLOCKED_RECT lockedRect;
		RECT r;
		r.left   = 0;
		r.top    = 0;
		r.right  = width;
		r.bottom = height;
		hresult = plainSurface->LockRect(&lockedRect, &r, flags);
		FATAL_DX_HR("LockRect failed %s", hresult);

		// let the user know where and how to write
		lockData.m_pixelData = lockedRect.pBits;
		lockData.m_pitch = lockedRect.Pitch;
		lockData.m_slicePitch = lockedRect.Pitch * lockData.getHeight();
	}

}

// ----------------------------------------------------------------------

void Direct3d9_TextureData::unlock(LockData &lockData)
{
	if (!m_d3dTexture)
	{
		delete [] reinterpret_cast<uint8*>(lockData.m_reserved);
		lockData.m_pitch      = 0;
		lockData.m_slicePitch = 0;
		lockData.m_pixelData  = 0;
		lockData.m_reserved   = NULL;
		return;
	}

        if (lockData.m_reserved)
        {
                // recover the source surface from the lock data and unlock it
                IDirect3DSurface9 * plainSurface = reinterpret_cast<IDirect3DSurface9 *>(lockData.m_reserved);
                HRESULT hresult = plainSurface->UnlockRect();
                FATAL_DX_HR("UnlockRect failed %s", hresult);

                if (!lockData.isReadOnly())
                {
                        // get the texture surface that we want to update
                        IDirect3DSurface9 * surface = 0;
                        if (m_engineTexture.isCubeMap())
                        {
                                hresult = static_cast<IDirect3DCubeTexture9*>(m_d3dTexture)->GetCubeMapSurface(Direct3d9::getD3dCubeFace(lockData.m_cubeFace), lockData.getLevel(), &surface);
                                FATAL_DX_HR("GetCubeMapSurface failed %s", hresult);
                        }
                        else
                        {
                                hresult = static_cast<IDirect3DTexture9*>(m_d3dTexture)->GetSurfaceLevel(lockData.getLevel(), &surface);
                                FATAL_DX_HR("GetSurfaceLevel failed %s", hresult);
                        }

                        // copy and convert the texture bits
                        RECT dest;
                        dest.left   = lockData.getX();
                        dest.top    = lockData.getY();
                        dest.right  = lockData.getX() + lockData.getWidth();
                        dest.bottom = lockData.getY() + lockData.getHeight();
                        hresult = D3DXLoadSurfaceFromSurface(surface, NULL, &dest, plainSurface, NULL, NULL, D3DX_FILTER_NONE, 0);
                        FATAL_DX_HR("D3DXLoadSurfaceFromSurface failed %s", hresult);

                        // free the resources
                        surface->Release();
                }

                plainSurface->Release();
        }
        else
        {
		// all we have to do is unlock
		if (m_engineTexture.isCubeMap())
		{
			HRESULT result = static_cast<IDirect3DCubeTexture9*>(m_d3dTexture)->UnlockRect(Direct3d9::getD3dCubeFace(lockData.m_cubeFace), lockData.getLevel());
			FATAL_DX_HR("failed to unlock rect %s", result);
		}
		else if (m_engineTexture.isVolumeMap())
		{
			HRESULT result = static_cast<IDirect3DVolumeTexture9*>(m_d3dTexture)->UnlockBox(lockData.getLevel());
			FATAL_DX_HR("failed to unlock box %s", result);
		}
		else
		{
			// unlock the surface
			HRESULT result = static_cast<IDirect3DTexture9*>(m_d3dTexture)->UnlockRect(lockData.getLevel());
			FATAL_DX_HR("failed to unlock rect %s", result);
		}
	}

	// clear out the lock data
	lockData.m_pitch      = 0;
	lockData.m_slicePitch = 0;
	lockData.m_pixelData  = 0;
	lockData.m_reserved   = NULL;
}

//----------------------------------------------------------------------

void  Direct3d9_TextureData::copyFrom(int surfaceLevel, TextureGraphicsData const & rhs, int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight)
{
	Direct3d9_TextureData const * const rhs_d3d = safe_cast<Direct3d9_TextureData const *>(&rhs);

	FATAL(this == rhs_d3d, ("Direct3d9_TextureData::copyFrom() src & dst may not be the same texture"));

	RECT const rectDst = { dstX, dstY, dstX + dstWidth, dstY + dstHeight };
	RECT const rectSrc = { srcX, srcY, srcX + srcWidth, srcY + srcHeight };

	// get the texture surface that we want to update
	IDirect3DSurface9 * surfaceSrc = 0;
	IDirect3DSurface9 * surfaceDst = 0;

	if (m_engineTexture.isCubeMap())
	{
		FATAL(true, ("Can't copy cube maps (yet)"));
	}
	else if (m_engineTexture.isVolumeMap())
	{
		FATAL(true, ("Can't copy volume maps (yet)"));
	}
	else
	{
		HRESULT const hresult = static_cast<IDirect3DTexture9*>(m_d3dTexture)->GetSurfaceLevel(surfaceLevel, &surfaceDst);
		FATAL_DX_HR("Direct3d9_TextureData::copyFrom() GetSurfaceLevel src failed %s", hresult);
	}

	if (rhs_d3d->m_engineTexture.isCubeMap())
	{
		FATAL(true, ("Direct3d9_TextureData::copyFrom() Can't copy cube maps (yet)"));
	}
	else if (rhs_d3d->m_engineTexture.isVolumeMap())
	{
		FATAL(true, ("Can't copy volume maps (yet)"));
	}
	else
	{
		HRESULT const hresult = static_cast<IDirect3DTexture9*>(rhs_d3d->m_d3dTexture)->GetSurfaceLevel(surfaceLevel, &surfaceSrc);
		FATAL_DX_HR("Direct3d9_TextureData::copyFrom() GetSurfaceLevel dst failed %s", hresult);
	}

	HRESULT const hresult = D3DXLoadSurfaceFromSurface(surfaceDst, NULL, &rectDst, surfaceSrc, NULL, &rectSrc, D3DX_FILTER_NONE, 0);
	FATAL_DX_HR("Direct3d9_TextureData::copyFrom() D3DXLoadSurfaceFromSurface failed %s", hresult); 

	surfaceDst->Release();
	surfaceSrc->Release();
}

// ----------------------------------------------------------------------

#ifdef _DEBUG

bool Direct3d9_TextureData::firstTimeUsedThisFrame() const
{
	if (m_lastUsedFrameNumber == Direct3d9::getFrameNumber())
		return false;

	m_lastUsedFrameNumber = Direct3d9::getFrameNumber();
	return true;
}

#endif

// ----------------------------------------------------------------------

#ifdef _DEBUG

int Direct3d9_TextureData::getMemorySize() const
{
	return m_memorySize;
}

#endif

// ----------------------------------------------------------------------

IDirect3DTexture9* create2dTexture(int width, int height, int mipmapLevelCount, TextureFormat textureFormat)
{
	IDirect3DDevice9* device = Direct3d9::getDevice();
	IDirect3DTexture9* newTexture = NULL;
	if(!device)
	{
		FATAL(true, ("Create2DTexture() : Tried to create a texture with an invalid device.\n"));
	}

	HRESULT result = device->CreateTexture(width, height, mipmapLevelCount, 0, translationTable[textureFormat], D3DPOOL_MANAGED, &newTexture, NULL);

	if(!newTexture || result != D3D_OK)
	{
		FATAL(true, ("Create2DTexture(): Failed to create a texture. D3D Error Code: [%i] \n", result));
	}

	return newTexture;
}

// ======================================================================
