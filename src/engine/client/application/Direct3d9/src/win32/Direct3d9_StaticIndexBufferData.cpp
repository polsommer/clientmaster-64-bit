// ======================================================================
//
// Direct3d9_StaticIndexBufferData.cpp
//
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "FirstDirect3d9.h"
#include "Direct3d9_StaticIndexBufferData.h"

#include "Direct3d9.h"
#include "Direct3d9_Metrics.h"
#include "sharedFoundation/MemoryBlockManager.h"

// ======================================================================

MemoryBlockManager *  Direct3d9_StaticIndexBufferData::ms_memoryBlockManager;

// ======================================================================

void Direct3d9_StaticIndexBufferData::install()
{
	ms_memoryBlockManager = new MemoryBlockManager("Direct3d9_StaticIndexBufferData", true, sizeof(Direct3d9_StaticIndexBufferData), 0, 0, 0);
}

// ----------------------------------------------------------------------

void Direct3d9_StaticIndexBufferData::remove()
{
	delete ms_memoryBlockManager;
	ms_memoryBlockManager = NULL;
}

// ----------------------------------------------------------------------

void *Direct3d9_StaticIndexBufferData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof (Direct3d9_StaticIndexBufferData), ("bad size"));
	DEBUG_FATAL(size != static_cast<size_t> (ms_memoryBlockManager->getElementSize()), ("installed with bad size"));
	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d9_StaticIndexBufferData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ======================================================================

Direct3d9_StaticIndexBufferData::Direct3d9_StaticIndexBufferData(const StaticIndexBuffer &indexBuffer)
:
	m_indexBuffer(indexBuffer),
	m_d3dIndexBuffer(NULL)
#ifdef _DEBUG
	,
	m_lastUsedFrameNumber(0),
	m_memorySize(0)
#endif
{
	const int indexCount = indexBuffer.getNumberOfIndices();
	int length = indexCount * static_cast<int>(sizeof(Index));
	if (length == 0)
	{
		WARNING(true, ("Static index buffer created with 0 indices; allocating minimal buffer to avoid D3DERR_INVALIDCALL.\n"));
		length = static_cast<int>(sizeof(Index));
	}
#ifdef _DEBUG
	m_memorySize = length;
#endif
	IDirect3DDevice9 *device = Direct3d9::getDevice();
	const D3DPOOL pool = Direct3d9::usesDirect3dEx() ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	const HRESULT hresult = device->CreateIndexBuffer(length, 0, D3DFMT_INDEX16, pool, &m_d3dIndexBuffer, NULL);
	FATAL_DX_HR("Could not create IB %s", hresult);

#ifdef _DEBUG
	Direct3d9_Metrics::indexBufferMemoryTotal += m_memorySize;
	Direct3d9_Metrics::indexBufferMemoryCreated += m_memorySize;
#endif
}

// ----------------------------------------------------------------------

Direct3d9_StaticIndexBufferData::~Direct3d9_StaticIndexBufferData()
{
#ifdef _DEBUG
	Direct3d9_Metrics::indexBufferMemoryTotal -= m_memorySize;
	Direct3d9_Metrics::indexBufferMemoryDestroyed += m_memorySize;
#endif

	IGNORE_RETURN(m_d3dIndexBuffer->Release());
}

// ----------------------------------------------------------------------

Index *Direct3d9_StaticIndexBufferData::lock(bool readOnly)
{
	const int numberOfIndices = m_indexBuffer.getNumberOfIndices();
	const int length          = numberOfIndices * sizeof(Index);

#ifdef _DEBUG
	if (!readOnly)
		Direct3d9_Metrics::indexBufferMemoryModified += m_memorySize;
#endif

	void *data = NULL;
	const HRESULT hresult = m_d3dIndexBuffer->Lock(0, length, &data, readOnly ? D3DLOCK_READONLY : 0);
	FATAL_DX_HR("Could not lock static IB %s", hresult);
	NOT_NULL(data);
	return reinterpret_cast<Index *>(data);
}

// ----------------------------------------------------------------------

void Direct3d9_StaticIndexBufferData::unlock()
{
	const HRESULT hresult = m_d3dIndexBuffer->Unlock();
	FATAL_DX_HR("Could not unlock IB %s", hresult);
}

// ----------------------------------------------------------------------

#ifdef _DEBUG

bool Direct3d9_StaticIndexBufferData::firstTimeUsedThisFrame() const
{
	if (m_lastUsedFrameNumber == Direct3d9::getFrameNumber())
		return false;

	m_lastUsedFrameNumber = Direct3d9::getFrameNumber();
	return true;
}

#endif

// ----------------------------------------------------------------------

#ifdef _DEBUG

int Direct3d9_StaticIndexBufferData::getMemorySize() const
{
	return m_memorySize;
}

#endif
// ======================================================================
