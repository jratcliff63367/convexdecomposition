#include "TestHACD.h"
#include "NvRenderDebug.h"
#include "HACD.h"
#include <stdio.h>
#include <stdlib.h>

class TestHACDImpl : public TestHACD, public HACD::ICallback
{
public:
	TestHACDImpl(void)
	{
		mHACD = HACD::HACD_API::create();
	}

	virtual ~TestHACDImpl(void)
	{
		mHACD->release();
	}

	virtual void render(RENDER_DEBUG::RenderDebug *renderDebug) final
	{
		uint32_t hullCount = mHACD->getHullCount();
		if (hullCount)
		{
			for (uint32_t i = 0; i < hullCount; i++)
			{
				const HACD::HACD_API::Hull *h = mHACD->getHull(i);
				if (h)
				{
					renderDebug->pushRenderState();
					for (uint32_t i = 0; i < h->mTriangleCount; i++)
					{
						uint32_t i1 = h->mIndices[i * 3 + 0];
						uint32_t i2 = h->mIndices[i * 3 + 1];
						uint32_t i3 = h->mIndices[i * 3 + 2];
						const float *p1 = &h->mVertices[i1 * 3];
						const float *p2 = &h->mVertices[i2 * 3];
						const float *p3 = &h->mVertices[i3 * 3];
						renderDebug->debugTri(p1, p2, p3);
					}
					renderDebug->popRenderState();
				}
			}
		}
	}

	virtual void decompose(HACD::HACD_API::Desc &desc)
	{
		desc.mCallback = this;
		uint32_t hullCount = mHACD->performHACD(desc);
		printf("Produced: %d convex hulls.\n", hullCount );

	}

	virtual void release(void)
	{
		delete this;
	}

	virtual void ReportProgress(const char *, float progress) final
	{
		printf("HACD::Progress(%0.2f)\n", progress);
	}

	virtual bool Cancelled() final
	{
		printf("HACD::Cancelled\n");
		return false;
	}

	HACD::HACD_API	*mHACD;
};

TestHACD *TestHACD::create(void)
{
	TestHACDImpl *t = new TestHACDImpl;
	return static_cast<TestHACD *>(t);
}


