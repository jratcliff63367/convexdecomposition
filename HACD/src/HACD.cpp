
#include "VHACD.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "MergeHulls.h"

#define HACD_ALLOC(x) malloc(x)
#define HACD_FREE(x) free(x)
#define HACD_ASSERT(x) assert(x)

#pragma warning(disable:4100)

namespace VHACD
{

inline double det(const double *p1,const double *p2,const double *p3)
{
	return  p1[0]*p2[1]*p3[2] + p2[0]*p3[1]*p1[2] + p3[0]*p1[1]*p2[2] -p1[0]*p3[1]*p2[2] - p2[0]*p1[1]*p3[2] - p3[0]*p2[1]*p1[2];
}


static double  fm_computeMeshVolume(const double *vertices,uint32_t tcount,const int *indices)
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



class MyHACD_API : public VHACD::IVHACD
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
		mVHACD = VHACD::CreateVHACD();
	}

	virtual ~MyHACD_API(void)
	{
		releaseHACD();
		mVHACD->Release();
	}

	virtual bool Compute(const double* const points,
		const unsigned int stridePoints,
		const unsigned int countPoints,
		const int* const triangles,
		const unsigned int strideTriangles,
		const unsigned int countTriangles,
		const Parameters& _desc) final
	{
		uint32_t ret = 0;

		mCallback = _desc.m_callback;

		releaseHACD();
		IVHACD::Parameters desc = _desc;

		if ( countPoints )
		{
			bool ok = mVHACD->Compute(points, stridePoints, countPoints, triangles, strideTriangles, countTriangles, desc);
			if (ok)
			{
				ret = mVHACD->GetNConvexHulls();
				mHullCount = ret;
				mHulls = new IVHACD::ConvexHull[mHullCount];

				for (unsigned int i = 0; i < ret; i++)
				{
					VHACD::IVHACD::ConvexHull vhull;
					mVHACD->GetConvexHull(i, vhull);
					VHACD::IVHACD::ConvexHull h;
					h.m_nPoints = vhull.m_nPoints;
					h.m_points = (double *)HACD_ALLOC(sizeof(double) * 3 * h.m_nPoints);
					memcpy(h.m_points, vhull.m_points, sizeof(double) * 3 * h.m_nPoints);
					h.m_nTriangles = vhull.m_nTriangles;
					h.m_triangles = (int *)HACD_ALLOC(sizeof(int) * 3 * h.m_nTriangles);
					memcpy(h.m_triangles, vhull.m_triangles, sizeof(int) * 3 * h.m_nTriangles);
					h.m_volume = fm_computeMeshVolume(h.m_points, h.m_nTriangles, h.m_triangles);
					fm_computCenter(h.m_nPoints, h.m_points, h.m_center);

					mHulls[i] = h;
				}
			}
		}

		ret = (uint32_t)mHullCount;

		if (ret && ret > (uint32_t)desc.m_maxConvexHulls)
		{
			MergeHullsInterface *mhi = createMergeHullsInterface();
			if (mhi)
			{
				if (desc.m_callback)
				{
					desc.m_callback->Update(1, 1, 0.1, "Merge ConvexHulls", "Gathering Convex Hulls");
				}

				MergeHullVector inputHulls;
				MergeHullVector outputHulls;
				for (uint32_t i = 0; i < ret; i++)
				{
					IVHACD::ConvexHull &h = mHulls[i];
					MergeHull mh;
					mh.mTriangleCount = h.m_nTriangles;
					mh.mVertexCount = h.m_nPoints;
					mh.mVertices = h.m_points;
					mh.mIndices = (uint32_t *)h.m_triangles;
					inputHulls.push_back(mh);
				}
				ret = mhi->mergeHulls(inputHulls, outputHulls, desc.m_maxConvexHulls, 0.01f + FLT_EPSILON, desc.m_maxNumVerticesPerCH, desc.m_callback);

				releaseHACD();

				if (desc.m_callback)
				{
					desc.m_callback->Update(1, 1, 0.2, "Merge Convex Hulls", "Gathering Merged Hulls");
				}

				mHullCount = uint32_t(outputHulls.size());
				mHulls = new IVHACD::ConvexHull[mHullCount];

				for (uint32_t i = 0; i < outputHulls.size(); i++)
				{
					IVHACD::ConvexHull h;
					const MergeHull &mh = outputHulls[i];
					h.m_nTriangles = mh.mTriangleCount;
					h.m_nPoints = mh.mVertexCount;
					h.m_triangles = (int *)HACD_ALLOC(sizeof(int) * 3 * h.m_nTriangles);
					h.m_points = (double *)HACD_ALLOC(sizeof(double) * 3 * h.m_nPoints);
					memcpy((uint32_t *)h.m_triangles, mh.mIndices, sizeof(uint32_t) * 3 * h.m_nTriangles);
					memcpy((double *)h.m_points, mh.mVertices, sizeof(double) * 3 * h.m_nPoints);

					h.m_volume = fm_computeMeshVolume(h.m_points, h.m_nTriangles, h.m_triangles);
					fm_computCenter(h.m_nPoints, h.m_points, h.m_center);

					mHulls[i] = h;
				}

				mhi->release();
			}
		}

		return ret ? true : false;
	}

	void releaseHull(VHACD::IVHACD::ConvexHull &h)
	{
		HACD_FREE((void *)h.m_triangles);
		HACD_FREE((void *)h.m_points);
		h.m_triangles = nullptr;
		h.m_points = nullptr;
	}

	virtual void GetConvexHull(const unsigned int index, VHACD::IVHACD::ConvexHull& ch) const final
	{
		if ( index < mHullCount )
		{
			ch = mHulls[index];
		}
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
		HACD_FREE(mVertices);
		mVertices = nullptr;
	}


	virtual void release(void) // release the HACD_API interface
	{
		delete this;
	}

	virtual uint32_t	getHullCount(void)
	{
		return mHullCount;
	}

	virtual void Cancel() final
	{

	}

	virtual bool Compute(const float* const points,
		const unsigned int stridePoints,
		const unsigned int countPoints,
		const int* const triangles,
		const unsigned int strideTriangles,
		const unsigned int countTriangles,
		const Parameters& params) final
	{

		HACD_FREE(mVertices);
		mVertices = (double *)HACD_ALLOC(sizeof(double)*countPoints * 3);
		const float *source = points;
		double *dest = mVertices;
		for (uint32_t i = 0; i < countPoints; i++)
		{
			dest[0] = source[0];
			dest[1] = source[1];
			dest[2] = source[2];
			dest += 3;
			source += stridePoints;
		}

		return Compute(mVertices, 3, countPoints, triangles, strideTriangles, countTriangles, params);

	}

	virtual unsigned int GetNConvexHulls() const final
	{
		return mHullCount;
	}

	virtual void Clean(void) final // release internally allocated memory
	{
		releaseHACD();
		mVHACD->Clean();
	}

	virtual void Release(void) final  // release IVHACD
	{
		delete this;
	}

	virtual bool OCLInit(void* const oclDevice,
		IUserLogger* const logger = 0) final
	{
		return mVHACD->OCLInit(oclDevice, logger);
	}
		
	virtual bool OCLRelease(IUserLogger* const logger = 0) final
	{
		return mVHACD->OCLRelease(logger);
	}

private:
	double							*mVertices{ nullptr };
	uint32_t						mHullCount{ 0 };
	VHACD::IVHACD::ConvexHull		*mHulls{ nullptr };
	VHACD::IVHACD::IUserCallback	*mCallback{ nullptr };
	VHACD::IVHACD					*mVHACD{ nullptr };
};

IVHACD* CreateVHACD_ASYNC(void)
{
	MyHACD_API *m = new MyHACD_API;
	return static_cast<IVHACD *>(m);
}


}; // end of VHACD namespace

