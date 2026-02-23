// ======================================================================
//
// TextureRendererTemplate.cpp
// copyright 2001 Sony Online Entertainment
// 
// ======================================================================

#include "clientTextureRenderer/FirstClientTextureRenderer.h"
#include "clientTextureRenderer/TextureRendererTemplate.h"

#include "clientGraphics/TextureList.h"
#include "clientTextureRenderer/TextureRendererList.h"
#include "sharedFoundation/CrcLowerString.h"

#include <algorithm>
#include <vector>

// ======================================================================

TextureRendererTemplate::TextureRendererTemplate(const char *name) :
	m_referenceCount(0),
	m_crcName(new CrcLowerString(name)),
	m_destinationPreferredWidth(0),
	m_destinationPreferredHeight(0),
	m_runtimeFormats(new TextureFormatContainer())
{
	// Ensure that we always have at least one valid runtime format.  This will
	// be replaced if the data provides an explicit format list, but it keeps
	// legacy content functional.
	m_runtimeFormats->push_back(TF_ARGB_8888);
}

// ----------------------------------------------------------------------

TextureRendererTemplate::~TextureRendererTemplate()
{
	delete m_runtimeFormats;
	delete m_crcName;
}

// ----------------------------------------------------------------------

void TextureRendererTemplate::release() const
{
	--m_referenceCount;
	DEBUG_WARNING(m_referenceCount < 0, ("bad reference handling %d\n", m_referenceCount));
	if (m_referenceCount == 0)
	{
		TextureRendererList::removeFromList(this);
		delete const_cast<TextureRendererTemplate*>(this);
	}
}

// ----------------------------------------------------------------------

int TextureRendererTemplate::getDestinationRuntimeFormatCount() const
{
	return static_cast<int>(m_runtimeFormats->size());
}

// ----------------------------------------------------------------------

TextureFormat TextureRendererTemplate::getDestinationRuntimeFormat(int index) const
{
	VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, index, static_cast<int>(m_runtimeFormats->size()));
	return (*m_runtimeFormats)[static_cast<size_t>(index)];
}

// ----------------------------------------------------------------------

void TextureRendererTemplate::setDestinationRuntimeFormats(const TextureFormat *runtimeFormats, int runtimeFormatCount)
{
	m_runtimeFormats->clear();

	if (runtimeFormats && runtimeFormatCount > 0)
	{
		m_runtimeFormats->reserve(static_cast<size_t>(runtimeFormatCount));

		for (int i = 0; i < runtimeFormatCount; ++i)
		{
			const TextureFormat format = runtimeFormats[i];

			if (format < 0 || format >= TF_Count)
			{
				DEBUG_WARNING(true, ("TextureRendererTemplate provided invalid runtime format index %d", format));
				continue;
			}

			if (std::find(m_runtimeFormats->begin(), m_runtimeFormats->end(), format) != m_runtimeFormats->end())
				continue;

			m_runtimeFormats->push_back(format);
		}
	}

	if (std::find(m_runtimeFormats->begin(), m_runtimeFormats->end(), TF_ARGB_8888) == m_runtimeFormats->end())
	{
		// Guarantee a sane fallback so we can still render even if the requested
		// high-end formats are unavailable on the current hardware.
		m_runtimeFormats->push_back(TF_ARGB_8888);
	}
}

// ----------------------------------------------------------------------

Texture *TextureRendererTemplate::fetchCompatibleTexture() const
{
	//-- create the texture of the requested size and format
	const int textureWidth  = getDestinationPreferredWidth();
	const int textureHeight = getDestinationPreferredHeight();

#if 1
	const int mipmapCount = 1;
#else
	const int mipmapCount        = 1 + GetFirstBitSet(static_cast<uint>(std::min(textureWidth, textureHeight)));
#endif
	const int runtimeFormatCount = getDestinationRuntimeFormatCount();
	DEBUG_FATAL(runtimeFormatCount <= 0, ("TextureRendererTemplate has no runtime formats"));

	return TextureList::fetch(0, textureWidth, textureHeight, mipmapCount, &(*m_runtimeFormats)[0], runtimeFormatCount);
}

// ======================================================================
