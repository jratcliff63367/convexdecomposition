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
#include "HACD.h"
#include "VHACD.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "MergeHulls.h"

#define HACD_ALLOC(x) malloc(x)
#define HACD_FREE(x) free(x)
#define HACD_ASSERT(x) assert(x)


namespace HACD
{

int32_t stringFormatV(char* dst, size_t dstSize, const char* src, va_list arg)
{
	return ::vsnprintf(dst, dstSize, src, arg);
}

inline int32_t stringFormat(char *dest,uint32_t size, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int ret = stringFormatV(dest, size, format, args);
	va_end(args);
	return ret;
}

inline double det(const double *p1,const double *p2,const double *p3)
{
	return  p1[0]*p2[1]*p3[2] + p2[0]*p3[1]*p1[2] + p3[0]*p1[1]*p2[2] -p1[0]*p3[1]*p2[2] - p2[0]*p1[1]*p3[2] - p3[0]*p2[1]*p1[2];
}


static double  fm_computeMeshVolume(const double *vertices,uint32_t tcount,const uint32_t *indices)
{
	double volume = 0;

	for (uint32_t i=0; i<tcount; i++,indices+=3)
	{
		const double *p1 = &vertices[ indices[0]*3 ];
		const double *p2 = &vertices[ indices[1]*3 ];
		const double *p3 = &vertices[ indices[2]*3 ];
		volume+=det(p1,p2,p3); // compute the volume of the tetrahedran relative to the origin.
	}

	volume*=(1.0f/6.0f);
	if ( volume < 0 )
		volume*=-1;
	return volume;
}

static void  fm_computCenter(uint32_t vcount,const double *vertices,double center[3])
{
	double bmin[3];
	double bmax[3];

	bmin[0] = vertices[0];
	bmin[1] = vertices[1];
	bmin[2] = vertices[2];

	bmax[0] = vertices[0];
	bmax[1] = vertices[1];
	bmax[2] = vertices[2];

	for (uint32_t i = 1; i < vcount; i++)
	{
		const double *v = &vertices[i * 3];

		if (v[0] < bmin[0]) bmin[0] = v[0];
		if (v[1] < bmin[1]) bmin[1] = v[1];
		if (v[2] < bmin[2]) bmin[2] = v[2];

		if (v[0] > bmax[0]) bmax[0] = v[0];
		if (v[1] > bmax[1]) bmax[1] = v[1];
		if (v[2] > bmax[2]) bmax[2] = v[2];
	}

	center[0] = (bmax[0] - bmin[0])*0.5f + bmin[0];
	center[1] = (bmax[1] - bmin[1])*0.5f + bmin[1];
	center[2] = (bmax[2] - bmin[2])*0.5f + bmin[2];

}



class MyHACD_API : public HACD_API, public VHACD::IVHACD::IUserCallback, public VHACD::IVHACD::IUserLogger
{
public:
	class Vec3
	{
	public:
		Vec3(void)
		{

		}
		Vec3(double _x,double _y,double _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}
		double x;
		double y;
		double z;
	};

	MyHACD_API(void)
	{
		
	}
	virtual ~MyHACD_API(void)
	{
		releaseHACD();
	}

	//
	virtual void Update(const double overallProgress,
		const double stageProgress,
		const double operationProgress,
		const char* const stage,
		const char* const operation) final
	{
		if (mCallback)
		{
			char scratch[512];
			stringFormat(scratch, sizeof(scratch), "VHACD::OverallProgress: %0.2f : StageProgress: %0.2f : Operation Progress: %0.2f : %s : %s\n", overallProgress, stageProgress, operationProgress, stage, operation);
			mCallback->ReportProgress(scratch, (float)operationProgress);
		}
	}

	virtual void Log(const char* const msg) final
	{
		if (mCallback)
		{
			mCallback->ReportProgress(msg, 0);
		}
	}

	virtual uint32_t	performHACD(const Desc &_desc) 
	{
		uint32_t ret = 0;

		mCallback = _desc.mCallback;

		if ( mCallback )
		{
			mCallback->ReportProgress("Starting HACD",1);
		}

		releaseHACD();

		Desc desc = _desc;

		if ( desc.mVertexCount )
		{
			VHACD::IVHACD *vhacd = VHACD::CreateVHACD();
			if (vhacd)
			{
				VHACD::IVHACD::Parameters p;
				p.m_concavity = desc.mConcavity;
				p.m_gamma = desc.mGamma;
				p.m_depth = desc.mDecompositionDepth;
				p.m_maxNumVerticesPerCH = desc.mMaxHullVertices;
				p.m_callback = this;
				p.m_logger = this;

				bool ok = vhacd->Compute(desc.mVertices, 3, desc.mVertexCount, (const int *const)desc.mIndices, 3, desc.mTriangleCount, p);

				if (ok)
				{
					ret = vhacd->GetNConvexHulls();
					mHullCount = ret;
					mHulls = new Hull[mHullCount];

					for (unsigned int i = 0; i < ret; i++)
					{
						VHACD::IVHACD::ConvexHull vhull;
						vhacd->GetConvexHull(i, vhull);
						Hull h;
						h.mVertexCount = vhull.m_nPoints;
						h.mVertices = (double *)HACD_ALLOC(sizeof(double) * 3 * h.mVertexCount);
						for (uint32_t j = 0; j < h.mVertexCount; j++)
						{
							double *dest = (double *)&h.mVertices[j * 3];
							dest[0] = (double)vhull.m_points[j * 3 + 0];
							dest[1] = (double)vhull.m_points[j * 3 + 1];
							dest[2] = (double)vhull.m_points[j * 3 + 2];
						}

						h.mTriangleCount = vhull.m_nTriangles;
						uint32_t *destIndices = (uint32_t *)HACD_ALLOC(sizeof(uint32_t) * 3 * h.mTriangleCount);
						h.mIndices = destIndices;

						for (uint32_t j = 0; j < h.mTriangleCount; j++)
						{
							destIndices[0] = vhull.m_triangles[j * 3 + 0];
							destIndices[1] = vhull.m_triangles[j * 3 + 1];
							destIndices[2] = vhull.m_triangles[j * 3 + 2];
							destIndices += 3;
						}

						h.mVolume = fm_computeMeshVolume(h.mVertices, h.mTriangleCount, h.mIndices);
						fm_computCenter(h.mVertexCount, h.mVertices, h.mCenter);

						mHulls[i] = h;
					}
				}

				vhacd->Release();
			}
		}

		ret = (uint32_t)mHullCount;

		if (ret && ret > desc.mMaxConvexHulls)
		{
			MergeHullsInterface *mhi = createMergeHullsInterface();
			if (mhi)
			{
				if (desc.mCallback)
				{
					desc.mCallback->ReportProgress("Gathering Input Hulls", 1);
				}

				MergeHullVector inputHulls;
				MergeHullVector outputHulls;
				for (uint32_t i = 0; i < ret; i++)
				{
					Hull &h = mHulls[i];
					MergeHull mh;
					mh.mTriangleCount = h.mTriangleCount;
					mh.mVertexCount = h.mVertexCount;
					mh.mVertices = h.mVertices;
					mh.mIndices = h.mIndices;
					inputHulls.push_back(mh);
				}
				ret = mhi->mergeHulls(inputHulls, outputHulls, desc.mMaxConvexHulls, 0.01f + FLT_EPSILON, desc.mMaxHullVertices, desc.mCallback);

				releaseHACD();

				if (desc.mCallback)
				{
					desc.mCallback->ReportProgress("Gathering Merged Hulls", 1);
				}

				mHullCount = uint32_t(outputHulls.size());
				mHulls = new Hull[mHullCount];

				for (uint32_t i = 0; i < outputHulls.size(); i++)
				{
					Hull h;
					const MergeHull &mh = outputHulls[i];
					h.mTriangleCount = mh.mTriangleCount;
					h.mVertexCount = mh.mVertexCount;
					h.mIndices = (uint32_t *)HACD_ALLOC(sizeof(uint32_t) * 3 * h.mTriangleCount);
					h.mVertices = (double *)HACD_ALLOC(sizeof(double) * 3 * h.mVertexCount);
					memcpy((uint32_t *)h.mIndices, mh.mIndices, sizeof(uint32_t) * 3 * h.mTriangleCount);
					memcpy((double *)h.mVertices, mh.mVertices, sizeof(double) * 3 * h.mVertexCount);

					h.mVolume = fm_computeMeshVolume(h.mVertices, h.mTriangleCount, h.mIndices);
					fm_computCenter(h.mVertexCount, h.mVertices, h.mCenter);

					mHulls[i] = h;
				}



				mhi->release();
			}
		}

		return ret;
	}

	void releaseHull(Hull &h)
	{
		HACD_FREE((void *)h.mIndices);
		HACD_FREE((void *)h.mVertices);
		h.mIndices = NULL;
		h.mVertices = NULL;
	}

	virtual const Hull		*getHull(uint32_t index)  const
	{
		const Hull *ret = NULL;
		if ( index < mHullCount )
		{
			ret = &mHulls[index];
		}
		return ret;
	}

	virtual void	releaseHACD(void) // release memory associated with the last HACD request
	{
		for (uint32_t i=0; i<mHullCount; i++)
		{
			releaseHull(mHulls[i]);
		}
		delete[]mHulls;
		mHulls = nullptr;
		mHullCount = 0;
	}


	virtual void release(void) // release the HACD_API interface
	{
		delete this;
	}

	virtual uint32_t	getHullCount(void)
	{
		return mHullCount;
	}

private:
	uint32_t				mHullCount{ 0 };
	Hull					*mHulls{ nullptr };
	ICallback				*mCallback{ nullptr };
};

HACD_API * HACD_API::create(void)
{
	MyHACD_API *m = new MyHACD_API;
	return static_cast<HACD_API *>(m);
}


};

