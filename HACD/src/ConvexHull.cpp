
#include "ConvexHull.h"
#include "WuQuantizer.h"

#include "vhacdICHull.h"

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

#include <math.h>
#include <float.h>
#include <string.h>


using namespace hacd;

namespace HACD
{

HullError HullLibrary::CreateConvexHull(const HullDesc       &desc,           // describes the input request
										HullResult           &result)         // contains the resulst
{
	HullError ret = QE_FAIL;

	uint32_t vcount = desc.mVcount;
	if ( vcount < 8 ) vcount = 8;

	float *vsource  = (float *) HACD_ALLOC( sizeof(float)*vcount*3 );
	float scale[3];
	float center[3];

	uint32_t ovcount;
	bool ok = NormalizeAndCleanupVertices(desc.mVcount,desc.mVertices, desc.mVertexStride, ovcount, vsource, desc.mNormalEpsilon, scale, center, desc.mMaxVertices*2, desc.mUseWuQuantizer ); // normalize point cloud, remove duplicates!
	if ( ok )
	{
		double *bigVertices = (double *)HACD_ALLOC(sizeof(double)*3*ovcount);
		for (uint32_t i=0; i<3*ovcount; i++)
		{
			bigVertices[i] = vsource[i];
		}
		VHACD::ICHull convexHull;
		convexHull.AddPoints((const VHACD::Vec3<double>*)bigVertices, ovcount);
		VHACD::ICHullError err = convexHull.Process(desc.mMaxVertices);
		if ( err == VHACD::ICHullError::ICHullErrorOK )
		{
			VHACD::TMMesh& mesh = convexHull.GetMesh();
			uint32_t vcount = (uint32_t)mesh.GetNVertices();
			if ( vcount <= ovcount )
			{
				uint32_t triangleCount = (uint32_t) mesh.GetNTriangles();
				uint32_t *indices = (uint32_t*)HACD_ALLOC(triangleCount*sizeof(uint32_t)*3);
				mesh.GetIFS((VHACD::Vec3<double> *)bigVertices,(VHACD::Vec3<int> *)indices);
				float *hullVertices = (float *)HACD_ALLOC( sizeof(float)*3*vcount );

				float *dest = hullVertices;
				const double *source = bigVertices;
				for (uint32_t i=0; i<vcount; i++)
				{
					dest[0] = (float)source[0]*scale[0]+center[0];
					dest[1] = (float)source[1]*scale[1]+center[1];
					dest[2] = (float)source[2]*scale[2]+center[2];
					dest+=3;
					source+=3;
				}
				// re-index triangle mesh so it refers to only used vertices, rebuild a new vertex table.
				float *vscratch = (float *) HACD_ALLOC( sizeof(float)*vcount*3 );
				BringOutYourDead(hullVertices,vcount,vscratch, ovcount, indices, triangleCount*3 );

				ret = QE_OK;

				result.mNumOutputVertices	= ovcount;
				result.mOutputVertices		= (float *)HACD_ALLOC( sizeof(float)*ovcount*3);
				result.mNumTriangles		= triangleCount;
				result.mIndices           = (uint32_t *) HACD_ALLOC( sizeof(uint32_t)*triangleCount*3);
				memcpy(result.mOutputVertices, vscratch, sizeof(float)*3*ovcount );
				memcpy(result.mIndices, indices, sizeof(uint32_t)*triangleCount*3);

				HACD_FREE(indices);
				HACD_FREE(vscratch);

			}
		}
		HACD_FREE(bigVertices);
	}

	HACD_FREE(vsource);

	return ret;
}



HullError HullLibrary::ReleaseResult(HullResult &result) // release memory allocated for this result, we are done with it.
{
	if ( result.mOutputVertices )
	{
		HACD_FREE(result.mOutputVertices);
		result.mOutputVertices = 0;
	}
	if ( result.mIndices )
	{
		HACD_FREE(result.mIndices);
		result.mIndices = 0;
	}
	return QE_OK;
}


bool  HullLibrary::NormalizeAndCleanupVertices(uint32_t svcount,
									const float *svertices,
									uint32_t /*stride*/,
									uint32_t &vcount,       // output number of vertices
									float *vertices,                 // location to store the results.
									float  /*normalepsilon*/,
									float *scale,
									float *center,
									uint32_t maxVertices,
									bool useWuQuantizer)
{
	bool ret = false;

	WuQuantizer *wq = createWuQuantizer();
	if ( wq )
	{
		const float *quantizedVertices;
		if ( useWuQuantizer )
		{
			quantizedVertices = wq->wuQuantize3D(svcount,svertices,false,maxVertices,vcount);
		}
		else
		{
			quantizedVertices = wq->kmeansQuantize3D(svcount,svertices,false,maxVertices,vcount);
		}
		if ( quantizedVertices )
		{
			memcpy(vertices,quantizedVertices,sizeof(float)*3*vcount);
			const float *_scale = wq->getDenormalizeScale();
			scale[0] = _scale[0];
			scale[1] = _scale[1];
			scale[2] = _scale[2];
			const float *_center = wq->getDenormalizeCenter();
			center[0] = _center[0];
			center[1] = _center[1];
			center[2] = _center[2];
			ret = true;
		}
		wq->release();
	}
	return ret;
}

void HullLibrary::BringOutYourDead(const float *verts,uint32_t vcount, float *overts,uint32_t &ocount,uint32_t *indices,uint32_t indexcount)
{
	uint32_t *used = (uint32_t *)HACD_ALLOC(sizeof(uint32_t)*vcount);
	memset(used,0,sizeof(uint32_t)*vcount);

	ocount = 0;

	for (uint32_t i=0; i<indexcount; i++)
	{
		uint32_t v = indices[i]; // original array index

		HACD_ASSERT( v < vcount );

		if ( used[v] ) // if already remapped
		{
			indices[i] = used[v]-1; // index to new array
		}
		else
		{

			indices[i] = ocount;      // new index mapping

			overts[ocount*3+0] = verts[v*3+0]; // copy old vert to new vert array
			overts[ocount*3+1] = verts[v*3+1];
			overts[ocount*3+2] = verts[v*3+2];

			ocount++; // increment output vert count

			HACD_ASSERT( ocount <= vcount );

			used[v] = ocount; // assign new index remapping
		}
	}

	HACD_FREE(used);
}

//==================================================================================
HullError HullLibrary::CreateTriangleMesh(HullResult &answer,ConvexHullTriangleInterface *iface)
{
	HullError ret = QE_FAIL;


	const float *p            = answer.mOutputVertices;
	const uint32_t   *idx = answer.mIndices;
	uint32_t fcount       = answer.mNumTriangles;

	if ( p && idx && fcount )
	{
		ret = QE_OK;

		for (uint32_t i=0; i<fcount; i++)
		{
			uint32_t pcount = *idx++;

			uint32_t i1 = *idx++;
			uint32_t i2 = *idx++;
			uint32_t i3 = *idx++;

			const float *p1 = &p[i1*3];
			const float *p2 = &p[i2*3];
			const float *p3 = &p[i3*3];

			AddConvexTriangle(iface,p1,p2,p3);

			pcount-=3;
			while ( pcount )
			{
				i3 = *idx++;
				p2 = p3;
				p3 = &p[i3*3];

				AddConvexTriangle(iface,p1,p2,p3);
				pcount--;
			}

		}
	}

	return ret;
}

//==================================================================================
void HullLibrary::AddConvexTriangle(ConvexHullTriangleInterface *callback,const float *p1,const float *p2,const float *p3)
{
	ConvexHullVertex v1,v2,v3;

	#define TSCALE1 (1.0f/4.0f)

	v1.mPos[0] = p1[0];
	v1.mPos[1] = p1[1];
	v1.mPos[2] = p1[2];

	v2.mPos[0] = p2[0];
	v2.mPos[1] = p2[1];
	v2.mPos[2] = p2[2];

	v3.mPos[0] = p3[0];
	v3.mPos[1] = p3[1];
	v3.mPos[2] = p3[2];

	float n[3];
	ComputeNormal(n,p1,p2,p3);

	v1.mNormal[0] = n[0];
	v1.mNormal[1] = n[1];
	v1.mNormal[2] = n[2];

	v2.mNormal[0] = n[0];
	v2.mNormal[1] = n[1];
	v2.mNormal[2] = n[2];

	v3.mNormal[0] = n[0];
	v3.mNormal[1] = n[1];
	v3.mNormal[2] = n[2];

	const float *tp1 = p1;
	const float *tp2 = p2;
	const float *tp3 = p3;

	int32_t i1 = 0;
	int32_t i2 = 0;

	float nx = fabsf(n[0]);
	float ny = fabsf(n[1]);
	float nz = fabsf(n[2]);

	if ( nx <= ny && nx <= nz )
		i1 = 0;
	if ( ny <= nx && ny <= nz )
		i1 = 1;
	if ( nz <= nx && nz <= ny )
		i1 = 2;

	switch ( i1 )
	{
		case 0:
			if ( ny < nz )
				i2 = 1;
			else
				i2 = 2;
			break;
		case 1:
			if ( nx < nz )
				i2 = 0;
			else
				i2 = 2;
			break;
		case 2:
			if ( nx < ny )
				i2 = 0;
			else
				i2 = 1;
			break;
	}

	v1.mTexel[0] = tp1[i1]*TSCALE1;
	v1.mTexel[1] = tp1[i2]*TSCALE1;

	v2.mTexel[0] = tp2[i1]*TSCALE1;
	v2.mTexel[1] = tp2[i2]*TSCALE1;

	v3.mTexel[0] = tp3[i1]*TSCALE1;
	v3.mTexel[1] = tp3[i2]*TSCALE1;

	callback->ConvexHullTriangle(v3,v2,v1);
}

//==================================================================================
float HullLibrary::ComputeNormal(float *n,const float *A,const float *B,const float *C)
{
	float vx,vy,vz,wx,wy,wz,vw_x,vw_y,vw_z,mag;

	vx = (B[0] - C[0]);
	vy = (B[1] - C[1]);
	vz = (B[2] - C[2]);

	wx = (A[0] - B[0]);
	wy = (A[1] - B[1]);
	wz = (A[2] - B[2]);

	vw_x = vy * wz - vz * wy;
	vw_y = vz * wx - vx * wz;
	vw_z = vx * wy - vy * wx;

	mag = sqrtf((vw_x * vw_x) + (vw_y * vw_y) + (vw_z * vw_z));

	if ( mag < 0.000001f )
	{
		mag = 0;
	}
	else
	{
		mag = 1.0f/mag;
	}

	n[0] = vw_x * mag;
	n[1] = vw_y * mag;
	n[2] = vw_z * mag;

	return mag;
}

}; // End of namespace HACD