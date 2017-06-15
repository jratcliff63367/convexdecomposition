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

	void getExplodePosition(const float source[3], float dest[3], const float diff[3],const float center[3])
	{
		dest[0] = source[0] + diff[0] + center[0];
		dest[1] = source[1] + diff[1] + center[1];
		dest[2] = source[2] + diff[2] + center[2];
	}

	virtual void render(RENDER_DEBUG::RenderDebug *renderDebug, float explodeViewScale,const float center[3]) final
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

					float diff[3];

					diff[0] = h->mCenter[0] - center[0];
					diff[1] = h->mCenter[1] - center[1];
					diff[2] = h->mCenter[2] - center[2];

					diff[0] *= explodeViewScale;
					diff[1] *= explodeViewScale;
					diff[2] *= explodeViewScale;

					diff[0] -= h->mCenter[0];
					diff[1] -= h->mCenter[1];
					diff[2] -= h->mCenter[2];

					for (uint32_t i = 0; i < h->mTriangleCount; i++)
					{
						uint32_t i1 = h->mIndices[i * 3 + 0];
						uint32_t i2 = h->mIndices[i * 3 + 1];
						uint32_t i3 = h->mIndices[i * 3 + 2];

						const float *p1 = &h->mVertices[i1 * 3];
						const float *p2 = &h->mVertices[i2 * 3];
						const float *p3 = &h->mVertices[i3 * 3];

						float v1[3];
						float v2[3];
						float v3[3];

						getExplodePosition(p1, v1, diff, center);
						getExplodePosition(p2, v2, diff, center);
						getExplodePosition(p3, v3, diff, center);

						renderDebug->debugTri(v1, v2, v3);
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

	virtual void ReportProgress(const char *msg, float progress) final
	{
		printf("HACD::%s(%0.2f)\n", msg, progress);
	}

	virtual bool Cancelled() final
	{
		printf("HACD::Cancelled\n");
		return false;
	}

	virtual uint32_t getHullCount(void) const final
	{
		return mHACD ? mHACD->getHullCount() : 0;
	}

	HACD::HACD_API	*mHACD;
};

TestHACD *TestHACD::create(void)
{
	TestHACDImpl *t = new TestHACDImpl;
	return static_cast<TestHACD *>(t);
}


