#ifndef MERGE_HULLS_H

#define MERGE_HULLS_H

#include "HACD.h"
#include <vector>



namespace HACD
{

class MergeHull
{
public:
	uint32_t			mTriangleCount;
	uint32_t			mVertexCount;
	const double		*mVertices;
	const uint32_t		*mIndices;
};

typedef std::vector< MergeHull > MergeHullVector;

class MergeHullsInterface
{
public:
	// Merge these input hulls.
	virtual uint32_t mergeHulls(const MergeHullVector &inputHulls,
									MergeHullVector &outputHulls,
									uint32_t	mergeHullCount,
									double smallClusterThreshold,
									uint32_t maxHullVertices,
									HACD::ICallback *callback) = 0;


	virtual void release(void) = 0;

protected:
	virtual ~MergeHullsInterface(void)
	{

	}

};

MergeHullsInterface * createMergeHullsInterface(void);

}; // end of HACD namespace

#endif
