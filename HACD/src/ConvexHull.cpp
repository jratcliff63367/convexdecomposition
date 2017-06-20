
#include "ConvexHull.h"

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

		uint32_t ovcount = desc.mVcount;
		double *bigVertices = (double *)HACD_ALLOC(sizeof(double) * 3 * ovcount);
		const float *vsource = desc.mVertices;
		for (uint32_t i = 0; i < 3 * ovcount; i++)
		{
			bigVertices[i] = (double)vsource[i];
		}
		VHACD::ICHull convexHull;
		convexHull.AddPoints((const VHACD::Vec3<double>*)bigVertices, ovcount);
		VHACD::ICHullError err = convexHull.Process(desc.mMaxVertices);
		if (err == VHACD::ICHullError::ICHullErrorOK)
		{
			VHACD::TMMesh& mesh = convexHull.GetMesh();
			uint32_t vcount = (uint32_t)mesh.GetNVertices();
			if (vcount <= ovcount)
			{
				uint32_t triangleCount = (uint32_t)mesh.GetNTriangles();
				uint32_t *indices = (uint32_t*)HACD_ALLOC(triangleCount*sizeof(uint32_t) * 3);
				mesh.GetIFS((VHACD::Vec3<double> *)bigVertices, (VHACD::Vec3<int> *)indices);
				float *hullVertices = (float *)HACD_ALLOC(sizeof(float) * 3 * vcount);

				float *dest = hullVertices;
				const double *source = bigVertices;
				for (uint32_t i = 0; i < vcount; i++)
				{
					dest[0] = (float)source[0];
					dest[1] = (float)source[1];
					dest[2] = (float)source[2];
					dest += 3;
					source += 3;
				}
				// re-index triangle mesh so it refers to only used vertices, rebuild a new vertex table.
				float *vscratch = (float *)HACD_ALLOC(sizeof(float)*vcount * 3);
				BringOutYourDead(hullVertices, vcount, vscratch, vcount, indices, triangleCount * 3);

				ret = QE_OK;

				result.mNumOutputVertices = vcount;
				result.mOutputVertices = (float *)HACD_ALLOC(sizeof(float)*vcount * 3);
				result.mNumTriangles = triangleCount;
				result.mIndices = (uint32_t *)HACD_ALLOC(sizeof(uint32_t)*triangleCount * 3);
				memcpy(result.mOutputVertices, vscratch, sizeof(float) * 3 * vcount);
				memcpy(result.mIndices, indices, sizeof(uint32_t)*triangleCount * 3);

				HACD_FREE(indices);
				HACD_FREE(vscratch);

			}
		}
		HACD_FREE(bigVertices);

		return ret;
	}



	HullError HullLibrary::ReleaseResult(HullResult &result) // release memory allocated for this result, we are done with it.
	{
		if (result.mOutputVertices)
		{
			HACD_FREE(result.mOutputVertices);
			result.mOutputVertices = 0;
		}
		if (result.mIndices)
		{
			HACD_FREE(result.mIndices);
			result.mIndices = 0;
		}
		return QE_OK;
	}


	void HullLibrary::BringOutYourDead(const float *verts, uint32_t vcount, float *overts, uint32_t &ocount, uint32_t *indices, uint32_t indexcount)
	{
		uint32_t *used = (uint32_t *)HACD_ALLOC(sizeof(uint32_t)*vcount);
		memset(used, 0, sizeof(uint32_t)*vcount);

		ocount = 0;

		for (uint32_t i = 0; i < indexcount; i++)
		{
			uint32_t v = indices[i]; // original array index

			HACD_ASSERT(v < vcount);

			if (used[v]) // if already remapped
			{
				indices[i] = used[v] - 1; // index to new array
			}
			else
			{

				indices[i] = ocount;      // new index mapping

				overts[ocount * 3 + 0] = verts[v * 3 + 0]; // copy old vert to new vert array
				overts[ocount * 3 + 1] = verts[v * 3 + 1];
				overts[ocount * 3 + 2] = verts[v * 3 + 2];

				ocount++; // increment output vert count

				HACD_ASSERT(ocount <= vcount);

				used[v] = ocount; // assign new index remapping
			}
		}

		HACD_FREE(used);
	}

} // end of namespace