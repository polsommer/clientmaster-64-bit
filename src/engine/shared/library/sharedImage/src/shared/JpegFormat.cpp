// ======================================================================
//
// JpegFormat.cpp
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedImage/FirstSharedImage.h"
#include "sharedImage/JpegFormat.h"

#include <stdio.h>
extern "C"
{
#include "jpeglib.h"
}

#include <setjmp.h>

// ======================================================================

namespace JpegFormatNamespace
{
	struct JpegErrorHandler
	{
		jpeg_error_mgr m_errorManager;
		jmp_buf        m_jumpBuffer;
	};

	void handleJpegError(j_common_ptr cinfo)
	{
		JpegErrorHandler *const errorHandler = reinterpret_cast<JpegErrorHandler *>(cinfo->err);
		longjmp(errorHandler->m_jumpBuffer, 1);
	}

	struct SourceManager
	{
		jpeg_source_mgr m_sourceManager;
		const JOCTET   *m_data;
		size_t          m_size;
	};

	void initSource(j_decompress_ptr)
	{
	}

	boolean fillInputBuffer(j_decompress_ptr cinfo)
	{
		static const JOCTET endOfImageBuffer[2] = {0xff, JPEG_EOI};
		SourceManager *const sourceManager = reinterpret_cast<SourceManager *>(cinfo->src);
		sourceManager->m_sourceManager.next_input_byte = endOfImageBuffer;
		sourceManager->m_sourceManager.bytes_in_buffer = sizeof(endOfImageBuffer);
		return TRUE;
	}

	void skipInputData(j_decompress_ptr cinfo, long numBytes)
	{
		if (numBytes <= 0)
			return;

		SourceManager *const sourceManager = reinterpret_cast<SourceManager *>(cinfo->src);
		if (numBytes > static_cast<long>(sourceManager->m_sourceManager.bytes_in_buffer))
		{
			sourceManager->m_sourceManager.next_input_byte = sourceManager->m_data + sourceManager->m_size;
			sourceManager->m_sourceManager.bytes_in_buffer = 0;
		}
		else
		{
			sourceManager->m_sourceManager.next_input_byte += numBytes;
			sourceManager->m_sourceManager.bytes_in_buffer -= static_cast<size_t>(numBytes);
		}
	}

	void termSource(j_decompress_ptr)
	{
	}

	void setMemorySource(j_decompress_ptr cinfo, const uint8 *data, size_t dataSize, SourceManager &sourceManager)
	{
		sourceManager.m_data = reinterpret_cast<const JOCTET *>(data);
		sourceManager.m_size = dataSize;
		sourceManager.m_sourceManager.init_source = initSource;
		sourceManager.m_sourceManager.fill_input_buffer = fillInputBuffer;
		sourceManager.m_sourceManager.skip_input_data = skipInputData;
		sourceManager.m_sourceManager.resync_to_restart = jpeg_resync_to_restart;
		sourceManager.m_sourceManager.term_source = termSource;
		sourceManager.m_sourceManager.bytes_in_buffer = dataSize;
		sourceManager.m_sourceManager.next_input_byte = sourceManager.m_data;
		cinfo->src = &sourceManager.m_sourceManager;
	}
}

using namespace JpegFormatNamespace;

// ======================================================================

JpegFormat::JpegFormat()
:
	ImageFormat()
{
}

// ----------------------------------------------------------------------

JpegFormat::~JpegFormat()
{
}

// ----------------------------------------------------------------------

const char *JpegFormat::getName() const
{
	return "JPEG";
}

// ----------------------------------------------------------------------

bool JpegFormat::isValidImage(const char *filename) const
{
	if (!filename || !*filename)
	{
		REPORT_LOG(true, ("JpegFormat::isValidImage(): bad filename\n"));
		return false;
	}

	uint8 *buffer = 0;
	int bufferSize = 0;
	if (!loadFileCreateBuffer(filename, &buffer, &bufferSize))
		return false;

	bool isValid = false;
	if (bufferSize < 4)
	{
		REPORT_LOG(true, ("JpegFormat::isValidImage(): [%s] is too small to be a jpeg file\n", filename));
	}
	else if ((buffer[0] == 0xff) && (buffer[1] == 0xd8) && (buffer[2] == 0xff))
	{
		isValid = true;
	}
	else
	{
		REPORT_LOG(true, ("JpegFormat::isValidImage(): [%s] failed jpeg signature check\n", filename));
	}

	delete [] buffer;
	return isValid;
}

// ----------------------------------------------------------------------

bool JpegFormat::loadImage(const char *filename, Image **image) const
{
	return loadImageReformat(filename, image, Image::PF_nonStandard);
}

// ----------------------------------------------------------------------

bool JpegFormat::loadImageReformat(const char *filename, Image **image, Image::PixelFormat format) const
{
	if (!filename || !*filename)
	{
		REPORT_LOG(true, ("JpegFormat::loadImage(): bad filename\n"));
		return false;
	}

	if (!image)
	{
		REPORT_LOG(true, ("JpegFormat::loadImage(): null image pointer\n"));
		return false;
	}
	*image = 0;

	if ((format != Image::PF_nonStandard) && (format != Image::PF_bgr_888))
	{
		REPORT_LOG(true, ("JpegFormat::loadImage(): unsupported pixel format request [%d] for [%s]\n", static_cast<int>(format), filename));
		return false;
	}

	uint8 *buffer = 0;
	int bufferSize = 0;
	if (!loadFileCreateBuffer(filename, &buffer, &bufferSize))
		return false;

	if (bufferSize < 4)
	{
		REPORT_LOG(true, ("JpegFormat::loadImage(): [%s] is too small to be a jpeg file\n", filename));
		delete [] buffer;
		return false;
	}

	jpeg_decompress_struct cinfo;
	JpegErrorHandler jpegErrorHandler;
	cinfo.err = jpeg_std_error(&jpegErrorHandler.m_errorManager);
	jpegErrorHandler.m_errorManager.error_exit = handleJpegError;

	if (setjmp(jpegErrorHandler.m_jumpBuffer))
	{
		REPORT_LOG(true, ("JpegFormat::loadImage(): invalid jpeg data in [%s]\n", filename));
		jpeg_destroy_decompress(&cinfo);
		delete [] buffer;
		return false;
	}

	jpeg_create_decompress(&cinfo);
	SourceManager sourceManager;
	setMemorySource(&cinfo, buffer, static_cast<size_t>(bufferSize), sourceManager);

	const int headerReadResult = jpeg_read_header(&cinfo, TRUE);
	if (headerReadResult != JPEG_HEADER_OK)
	{
		REPORT_LOG(true, ("JpegFormat::loadImage(): failed to read jpeg header for [%s]\n", filename));
		jpeg_destroy_decompress(&cinfo);
		delete [] buffer;
		return false;
	}

	cinfo.out_color_space = JCS_RGB;
	IGNORE_RETURN(jpeg_start_decompress(&cinfo));

	if ((cinfo.output_width == 0) || (cinfo.output_height == 0) || (cinfo.output_components != 3))
	{
		REPORT_LOG(true, ("JpegFormat::loadImage(): unexpected jpeg output layout for [%s] (w=%u h=%u components=%d)\n", filename, cinfo.output_width, cinfo.output_height, cinfo.output_components));
		jpeg_destroy_decompress(&cinfo);
		delete [] buffer;
		return false;
	}

	Image *const newImage = new Image();
	newImage->setDimensions(static_cast<int>(cinfo.output_width), static_cast<int>(cinfo.output_height), 24, 3);
	newImage->setPixelInformation(0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000);

	uint8 *const destinationBuffer = newImage->lock();
	const size_t scanlineSize = static_cast<size_t>(cinfo.output_width) * 3;
	JSAMPARRAY rowArray = (*cinfo.mem->alloc_sarray)(reinterpret_cast<j_common_ptr>(&cinfo), JPOOL_IMAGE, static_cast<JDIMENSION>(scanlineSize), 1);

	for (JDIMENSION row = 0; row < cinfo.output_height; ++row)
	{
		IGNORE_RETURN(jpeg_read_scanlines(&cinfo, rowArray, 1));
		const uint8 *const source = rowArray[0];
		uint8 *const destination = destinationBuffer + (row * scanlineSize);
		for (size_t x = 0; x < static_cast<size_t>(cinfo.output_width); ++x)
		{
			destination[x * 3 + 0] = source[x * 3 + 2];
			destination[x * 3 + 1] = source[x * 3 + 1];
			destination[x * 3 + 2] = source[x * 3 + 0];
		}
	}
	newImage->unlock();

	IGNORE_RETURN(jpeg_finish_decompress(&cinfo));
	jpeg_destroy_decompress(&cinfo);
	delete [] buffer;

	*image = newImage;
	return true;
}

// ----------------------------------------------------------------------

int JpegFormat::getCommonExtensionCount() const
{
	return 2;
}

// ----------------------------------------------------------------------

const char *JpegFormat::getCommonExtension(int index) const
{
	VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, index, 2);
	if (index == 0)
		return "jpg";

	return "jpeg";
}

// ======================================================================
