#pragma once
#include <algorithm>

namespace Parallel {
	template<typename Index>
	bool IsLastSplitRangeForStepOne(Index first, Index last, Index step) {
		return last - first == 1 && step == 1;
	}

	template<typename Index>
	Index FetchRangeLength(Index first, Index last) {
		return (last - first) + 1;
	}

	template<typename Index>
	Index FetchMidRangeFor(Index first, Index last, Index step) {
		Index iterations = FetchRangeLength(first, last);
		int tasks = ceil((float)iterations / (float)step);
		int c = ceil((float)tasks / 2.0);

		Index center = 0;
		if (IsLastSplitRangeForStepOne<Index>(first, last, step)) {
			center = first;
		} else {
			center = first + (step - 1);
			for (int i = 0; i < c - 1; i++) {
				center += step;
			}
		}

		return center;
	}
}