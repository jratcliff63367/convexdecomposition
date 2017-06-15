#ifndef TEST_HACD_H
#define TEST_HACD_H

#include <stdint.h>
#include "HACD.h"

namespace RENDER_DEBUG
{
	class RenderDebug;
}



class TestHACD
{
public:
	static TestHACD *create(void);

	virtual void decompose(HACD::HACD_API::Desc &desc) = 0;

	virtual void render(RENDER_DEBUG::RenderDebug *renderDebug,float explodeViewScale,const float center[3]) = 0;

	virtual uint32_t getHullCount(void) const = 0;

	virtual void release(void) = 0;
protected:
	virtual ~TestHACD(void)
	{
	}
};

#endif
