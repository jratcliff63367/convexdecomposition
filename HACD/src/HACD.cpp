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
#include "PlatformConfigHACD.h"
#include "MergeHulls.h"
#include "ConvexDecomposition.h"

using namespace hacd;

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

inline float det(const float *p1,const float *p2,const float *p3)
{
	return  p1[0]*p2[1]*p3[2] + p2[0]*p3[1]*p1[2] + p3[0]*p1[1]*p2[2] -p1[0]*p3[1]*p2[2] - p2[0]*p1[1]*p3[2] - p3[0]*p2[1]*p1[2];
}


static float  fm_computeMeshVolume(const float *vertices,uint32_t tcount,const uint32_t *indices)
{
	float volume = 0;

	for (uint32_t i=0; i<tcount; i++,indices+=3)
	{
		const float *p1 = &vertices[ indices[0]*3 ];
		const float *p2 = &vertices[ indices[1]*3 ];
		const float *p3 = &vertices[ indices[2]*3 ];
		volume+=det(p1,p2,p3); // compute the volume of the tetrahedran relative to the origin.
	}

	volume*=(1.0f/6.0f);
	if ( volume < 0 )
		volume*=-1;
	return volume;
}

static void  fm_computCenter(uint32_t vcount,const float *vertices,float center[3])
{
	float bmin[3];
	float bmax[3];

	bmin[0] = vertices[0];
	bmin[1] = vertices[1];
	bmin[2] = vertices[2];

	bmax[0] = vertices[0];
	bmax[1] = vertices[1];
	bmax[2] = vertices[2];

	for (uint32_t i = 1; i < vcount; i++)
	{
		const float *v = &vertices[i * 3];

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



class MyHACD_API : public HACD_API, public UANS::UserAllocated, public VHACD::IVHACD::IUserCallback, public VHACD::IVHACD::IUserLogger
{
public:
	class Vec3
	{
	public:
		Vec3(void)
		{

		}
		Vec3(float _x,float _y,float _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}
		float x;
		float y;
		float z;
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

	void normalizeInputMesh(Desc &desc,Vec3 &inputScale,Vec3 &inputCenter)
	{
		const float *source = desc.mVertices;

		Vec3 bmin(0,0,0),bmax(0,0,0);

		for (uint32_t i=0; i<desc.mVertexCount; i++)
		{
			const Vec3 &v = *(const Vec3 *)source;
			if ( i == 0 )
			{
				bmin = v;
				bmax = v;
			}
			else
			{
				if ( v.x < bmin.x ) bmin.x = v.x;
				if ( v.y < bmin.y ) bmin.y = v.y;
				if ( v.z < bmin.z ) bmin.z = v.z;

				if ( v.x > bmax.x ) bmax.x = v.x;
				if ( v.y > bmax.y ) bmax.y = v.y;
				if ( v.z > bmax.z ) bmax.z = v.z;

			}
			source+=3;
		}

		inputCenter.x = (bmin.x+bmax.x)*0.5f;
		inputCenter.y = (bmin.y+bmax.y)*0.5f;
		inputCenter.z = (bmin.z+bmax.z)*0.5f;

		float dx = bmax.x - bmin.x;
		float dy = bmax.y - bmin.y;
		float dz = bmax.z - bmin.z;

		if ( dx > 0 )
		{
			inputScale.x = 1.0f / dx;
		}
		else
		{
			inputScale.x = 1;
		}

		if ( dy > 0 )
		{
			inputScale.y = 1.0f / dy;
		}
		else
		{
			inputScale.y = 1;
		}

		if ( dz > 0 )
		{
			inputScale.z = 1.0f / dz;
		}
		else
		{
			inputScale.z = 1;
		}

		source = desc.mVertices;
		float *dest = (float *)HACD_ALLOC( sizeof(float)*3*desc.mVertexCount );
		desc.mVertices = dest;
		for (uint32_t i=0; i<desc.mVertexCount; i++)
		{

			dest[0] = (source[0]-inputCenter.x)*inputScale.x;
			dest[1] = (source[1]-inputCenter.y)*inputScale.y;
			dest[2] = (source[2]-inputCenter.z)*inputScale.z;

			dest+=3;
			source+=3;
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




		float *tempPositions = nullptr; // temp memory holding remapped vertex positions
		uint32_t *tempIndices = nullptr; // temp memory holding remapped triangle indices

		// This method scans the input mesh for duplicate vertices.
		if ( desc.mRemoveDuplicateVertices )
		{
			if ( desc.mCallback )
			{
				desc.mCallback->ReportProgress("Removing duplicate vertices",1);
			}

			tempPositions = (float *)HACD_ALLOC(sizeof(float)*desc.mVertexCount*3); // room to hold all of the input vertex positions
			tempIndices = (uint32_t *)HACD_ALLOC(sizeof(uint32_t)*desc.mTriangleCount*3); // room to hold all of the triangle indices

			desc.mVertices	= tempPositions;	// the remapped vertex position data
			desc.mIndices	= tempIndices;	// the remapped triangle indices

			uint32_t removeCount = 0;

			desc.mVertexCount = 0;
			uint32_t *remapPositions = (uint32_t *)HACD_ALLOC(sizeof(uint32_t)*_desc.mVertexCount);

			// Scan each input position and see if it duplicates an already defined vertex position
			for (uint32_t i=0; i<_desc.mVertexCount; i++)
			{

				const float *p1 = &_desc.mVertices[i*3]; // see if this position is already represented in out vertex list.
				// Iterate through all positions we have already defined

				bool found = false;
				for (uint32_t j=0; j<desc.mVertexCount; j++)
				{
					const float *p2 = &desc.mVertices[j*3];		// an existing psotion

					float dx = p1[0] - p2[0];
					float dy = p1[1] - p2[1];
					float dz = p1[2] - p2[2];

					float dist = dx*dx+dy*dy+dz*dz;	// Compute teh squared distance between this position and a previously defined position

					if ( dist < (0.001f*0.001f)) // if the position is essentially identical; less than 1mm different location then we do not add it.
					{
						found = true;
						remapPositions[i] = j;	// remap the original source position I to the new index position J
						removeCount++;	// increment the counter indicating the number of duplicates we have fou8nd
					}
				}
				if ( !found ) // if no duplicate was found; then this is a unique input position and we add it to the output.
				{
					remapPositions[i] = desc.mVertexCount;		// This input position 'I' remaps to the current output position location desc.mVertexCount
					float *p2 = &tempPositions[desc.mVertexCount*3]; // This is the destination for the unique input position.
					p2[0] = p1[0];	// copy X
					p2[1] = p1[1];	// copy Y
					p2[2] = p1[2];	// copy Z
					desc.mVertexCount++;	// increment the number of vertices in the new output
				}
			}
			// now we need to build the remapped index table.
			for (uint32_t i=0; i<desc.mTriangleCount*3; i++)
			{
				tempIndices[i] = remapPositions[ _desc.mIndices[i] ];
			}
			HACD_FREE(remapPositions);
			if ( desc.mCallback )
			{
				char scratch[512];
				HACD_SPRINTF_S(scratch,512,"Removed %d duplicate vertices.", removeCount );
				desc.mCallback->ReportProgress(scratch,1);
			}
		}
		Vec3 inputScale(1, 1, 1);
		Vec3 inputCenter(0, 0, 0);
		float *normalizeInputVertices = nullptr;
		if (desc.mNormalizeInputMesh)
		{
			normalizeInputMesh(desc, inputScale, inputCenter);
			normalizeInputVertices = (float *)desc.mVertices;
		}
		if ( desc.mVertexCount )
		{
			if (desc.mMode == HACD::HACD_API::USE_ACD)
			{
				CONVEX_DECOMPOSITION::ConvexDecomposition *cd = CONVEX_DECOMPOSITION::createConvexDecomposition();
				CONVEX_DECOMPOSITION::DecompDesc dcompDesc;
				dcompDesc.mIndices = desc.mIndices;
				dcompDesc.mVertices = desc.mVertices;
				dcompDesc.mTcount = desc.mTriangleCount;
				dcompDesc.mVcount = desc.mVertexCount;
				dcompDesc.mMaxVertices = desc.mMaxHullVertices;
				dcompDesc.mDepth = desc.mDecompositionDepthACD;
				dcompDesc.mCpercent = desc.mConcavity * 10;
				dcompDesc.mMeshVolumePercent = desc.mMeshVolumePercent;
				dcompDesc.mCallback = desc.mCallback;

				if (desc.mMaxConvexHulls == 1) // if we only want a single hull output then set the decomposition depth to zero!
				{
					dcompDesc.mDepth = 0;
				}

				ret = cd->performConvexDecomposition(dcompDesc);

				releaseHACD();
				mHullCount = ret;
				mHulls = new Hull[mHullCount];

				Vec3 recipInputScale;
				recipInputScale.x = 1.0f / inputScale.x;
				recipInputScale.y = 1.0f / inputScale.y;
				recipInputScale.z = 1.0f / inputScale.z;

				for (uint32_t i = 0; i < ret; i++)
				{
					CONVEX_DECOMPOSITION::ConvexResult *result = cd->getConvexResult(i, true);
					Hull h;

					h.mVertices = result->mHullVertices;
					h.mVertexCount = result->mHullVcount;

					for (uint32_t j = 0; j < h.mVertexCount; j++)
					{
						float *dest = (float *)&h.mVertices[j * 3];

						dest[0] = (dest[0] * recipInputScale.x) + inputCenter.x;
						dest[1] = (dest[1] * recipInputScale.y) + inputCenter.y;
						dest[2] = (dest[2] * recipInputScale.z) + inputCenter.z;

					}

					h.mIndices = result->mHullIndices;
					h.mTriangleCount = result->mHullTcount;
					h.mVolume = fm_computeMeshVolume(h.mVertices, h.mTriangleCount, h.mIndices);
					fm_computCenter(h.mVertexCount, h.mVertices, h.mCenter);
					mHulls[i] = h;
				}
			}
			else
			{
				VHACD::IVHACD *vhacd = VHACD::CreateVHACD();
				if (vhacd)
				{
					VHACD::IVHACD::Parameters p;
					p.m_concavity = desc.mConcavity;
					p.m_gamma = desc.mGamma;
					p.m_depth = desc.mDecompositionDepthVHACD;
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
							h.mVertices = (float *)HACD_ALLOC(sizeof(float) * 3 * h.mVertexCount);

							Vec3 recipInputScale;
							recipInputScale.x = 1.0f / inputScale.x;
							recipInputScale.y = 1.0f / inputScale.y;
							recipInputScale.z = 1.0f / inputScale.z;

							for (uint32_t j = 0; j < h.mVertexCount; j++)
							{
								float *dest = (float *)&h.mVertices[j * 3];

								dest[0] = (float)vhull.m_points[j * 3 + 0];
								dest[1] = (float)vhull.m_points[j * 3 + 1];
								dest[2] = (float)vhull.m_points[j * 3 + 2];

								dest[0] = (dest[0] * recipInputScale.x) + inputCenter.x;
								dest[1] = (dest[1] * recipInputScale.y) + inputCenter.y;
								dest[2] = (dest[2] * recipInputScale.z) + inputCenter.z;

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

				mHullCount = outputHulls.size();
				mHulls = new Hull[mHullCount];

				for (uint32_t i = 0; i < outputHulls.size(); i++)
				{
					Hull h;
					const MergeHull &mh = outputHulls[i];
					h.mTriangleCount = mh.mTriangleCount;
					h.mVertexCount = mh.mVertexCount;
					h.mIndices = (uint32_t *)HACD_ALLOC(sizeof(uint32_t) * 3 * h.mTriangleCount);
					h.mVertices = (float *)HACD_ALLOC(sizeof(float) * 3 * h.mVertexCount);
					memcpy((uint32_t *)h.mIndices, mh.mIndices, sizeof(uint32_t) * 3 * h.mTriangleCount);
					memcpy((float *)h.mVertices, mh.mVertices, sizeof(float) * 3 * h.mVertexCount);

					h.mVolume = fm_computeMeshVolume(h.mVertices, h.mTriangleCount, h.mIndices);
					fm_computCenter(h.mVertexCount, h.mVertices, h.mCenter);

					mHulls[i] = h;
				}



				mhi->release();
			}
		}

		HACD_FREE(normalizeInputVertices);
		HACD_FREE(tempPositions);
		HACD_FREE(tempIndices);


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
	MyHACD_API *m = HACD_NEW(MyHACD_API);
	return static_cast<HACD_API *>(m);
}


};

