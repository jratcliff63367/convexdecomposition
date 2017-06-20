#ifndef HACD_H

#define HACD_H

#include <stdint.h>

/*!
**
** Copyright (c) 2014 by John W. Ratcliff mailto:jratcliffscarab@gmail.com
**
**
** The MIT license:
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is furnished
** to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.

** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
** WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


**
** If you find this code snippet useful; you can tip me at this bitcoin address:
**
** BITCOIN TIP JAR: "1BT66EoaGySkbY9J6MugvQRhMMXDwPxPya"
**

*/

namespace HACD
{

class ICallback
{
public:
	virtual void ReportProgress(const char *, float progress) = 0;
	virtual bool Cancelled() = 0;
};

class HACD_API
{
public:
	class Desc
	{
	public:
		Desc(void)
		{
			init();
		}

		uint32_t			mTriangleCount;
		uint32_t			mVertexCount;
		const double		*mVertices;
		const uint32_t		*mIndices;
		uint32_t			mMaxHullVertices;
		double				mConcavity;
		double				mGamma;
		uint32_t			mDecompositionDepth; // 
		ICallback*			mCallback;
		uint32_t			mMaxConvexHulls;	// Maximum number of convex hulls
		void init(void)
		{
			mMaxConvexHulls = 512;
			mDecompositionDepth = 20;
			mTriangleCount = 0;
			mVertexCount = 0;
			mVertices = nullptr;
			mIndices = nullptr;
			mMaxHullVertices = 64;
			mConcavity = 0.2f;
			mGamma = 0.0005f;
			mCallback = nullptr;
		}
	};

	class Hull	
	{
	public:
		uint32_t			mTriangleCount;
		uint32_t			mVertexCount;
		const double		*mVertices;
		const uint32_t		*mIndices;
		double				mCenter[3];		// center of this convex hull
		double				mVolume;
	};

	static HACD_API * create(void);

	virtual uint32_t		performHACD(const Desc &desc) = 0;
	virtual uint32_t		getHullCount(void) = 0;
	virtual const Hull		*getHull(uint32_t index) const = 0;
	virtual void			releaseHACD(void) = 0; // release memory associated with the last HACD request
	

	virtual void			release(void) = 0; // release the HACD_API interface
protected:
	virtual ~HACD_API(void)
	{

	}
};



};

#endif
